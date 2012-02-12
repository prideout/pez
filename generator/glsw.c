
#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#pragma warning(disable:4996) // allow "fopen"
#endif

///////////////////////////////////////////////////////////////////////////////
// PRIVATE TYPES

typedef struct glswListRec
{
    bstring Key;
    bstring Value;
    struct glswListRec* Next;
} glswList;

typedef struct glswContextRec
{
    bstring ErrorMessage;
    glswList* TokenMap;
    glswList* ShaderMap;
    glswList* LoadedEffects;
    glswList* PathList;
} glswContext;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE GLOBALS

static glswContext* __glsw__Context = 0;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS

static int __glsw__Alphanumeric(char c)
{
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '.';
}

static void __glsw__FreeList(glswList* pNode)
{
    while (pNode)
    {
        glswList* pNext = pNode->Next;
        bdestroy(pNode->Key);
        bdestroy(pNode->Value);
        free(pNode);
        pNode = pNext;
    }
}

static bstring __glsw__LoadEffectContents(glswContext* gc, bstring effectName)
{
    FILE* fp = 0;
    bstring effectFile, effectContents;
    glswList* pPathList = gc->PathList;
    
    while (pPathList)
    {
        effectFile = bstrcpy(effectName);
        binsert(effectFile, 0, pPathList->Key, '?');
        bconcat(effectFile, pPathList->Value);

        fp = fopen((const char*) effectFile->data, "rb");
        if (fp)
        {
            break;
        }
            
        pPathList = pPathList->Next;
    }

    if (!fp)
    {
        bdestroy(gc->ErrorMessage);
        gc->ErrorMessage = bformat("Unable to open effect file '%s'.", effectFile->data);
        bdestroy(effectFile);
        return 0;
    }
    
    // Add a new entry to the front of gc->LoadedEffects
    {
        glswList* temp = gc->LoadedEffects;
        gc->LoadedEffects = (glswList*) calloc(sizeof(glswList), 1);
        gc->LoadedEffects->Key = bstrcpy(effectName);
        gc->LoadedEffects->Next = temp;
    }
    
    // Read in the effect file
    effectContents = bread((bNread) fread, fp);
    fclose(fp);
    bdestroy(effectFile);
    return effectContents;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS

int glswInit()
{
    if (__glsw__Context)
    {
        bdestroy(__glsw__Context->ErrorMessage);
        __glsw__Context->ErrorMessage = bfromcstr("Already initialized.");
        return 0;
    }

    __glsw__Context = (glswContext*) calloc(sizeof(glswContext), 1);
    
    glswAddPath("", "");

    return 1;
}

int glswShutdown()
{
    glswContext* gc = __glsw__Context;

    if (!gc)
    {
        return 0;
    }

    bdestroy(gc->ErrorMessage);

    __glsw__FreeList(gc->TokenMap);
    __glsw__FreeList(gc->ShaderMap);
    __glsw__FreeList(gc->LoadedEffects);
    __glsw__FreeList(gc->PathList);

    free(gc);
    __glsw__Context = 0;

    return 1;
}

int glswAddPath(const char* pathPrefix, const char* pathSuffix)
{
    glswContext* gc = __glsw__Context;
    glswList* temp;

    if (!gc)
    {
        return 0;
    }

    temp = gc->PathList;
    gc->PathList = (glswList*) calloc(sizeof(glswList), 1);
    gc->PathList->Key = bfromcstr(pathPrefix);
    gc->PathList->Value = bfromcstr(pathSuffix);
    gc->PathList->Next = temp;

    return 1;
}

const char* glswGetShader(const char* pEffectKey)
{
    glswContext* gc = __glsw__Context;
    bstring effectKey;
    glswList* closestMatch = 0;
    struct bstrList* tokens;
    bstring effectName;
    glswList* pLoadedEffect;
    glswList* pShaderEntry;
    bstring shaderKey = 0;

    if (!gc)
    {
        return 0;
    }

    // Extract the effect name from the effect key
    effectKey = bfromcstr(pEffectKey);
    tokens = bsplit(effectKey, '.');
    if (!tokens || !tokens->qty)
    {
        bdestroy(gc->ErrorMessage);
        gc->ErrorMessage = bformat("Malformed effect key key '%s'.", pEffectKey);
        bstrListDestroy(tokens);
        bdestroy(effectKey);
        return 0;
    }
    effectName = tokens->entry[0];

    // Check if we already loaded this effect file
    pLoadedEffect = gc->LoadedEffects;
    while (pLoadedEffect)
    {
        if (1 == biseq(pLoadedEffect->Key, effectName))
        {
            break;
        }
        pLoadedEffect = pLoadedEffect->Next;
    }

    // If we haven't loaded this file yet, load it in
    if (!pLoadedEffect)
    {
        bstring effectContents = __glsw__LoadEffectContents(gc, effectName);
        struct bstrList* lines = bsplit(effectContents, '\n');
        int lineNo;

        bdestroy(effectContents);
        effectContents = 0;

        for (lineNo = 0; lines && lineNo < lines->qty; lineNo++)
        {
            bstring line = lines->entry[lineNo];

            // If the line starts with "--", then it marks a new section
            if (blength(line) >= 2 && line->data[0] == '-' && line->data[1] == '-')
            {
                // Find the first character in [A-Za-z0-9_].
                int colNo;
                for (colNo = 2; colNo < blength(line); colNo++)
                {
                    char c = line->data[colNo];
                    if (__glsw__Alphanumeric(c))
                    {
                        break;
                    }
                }

                // If there's no alphanumeric character,
                // then this marks the start of a new comment block.
                if (colNo >= blength(line))
                {
                    bdestroy(shaderKey);
                    shaderKey = 0;
                }
                else
                {
                    // Keep reading until a non-alphanumeric character is found.
                    int endCol;
                    for (endCol = colNo; endCol < blength(line); endCol++)
                    {
                        char c = line->data[endCol];
                        if (!__glsw__Alphanumeric(c))
                        {
                            break;
                        }
                    }

                    bdestroy(shaderKey);
                    shaderKey = bmidstr(line, colNo, endCol - colNo);

                    // Add a new entry to the shader map.
                    {
                        glswList* temp = gc->ShaderMap;
                        gc->ShaderMap = (glswList*) calloc(sizeof(glswList), 1);
                        gc->ShaderMap->Key = bstrcpy(shaderKey);
                        gc->ShaderMap->Next = temp;
                        gc->ShaderMap->Value = bformat("#line %d\n", lineNo);

                        binsertch(gc->ShaderMap->Key, 0, 1, '.');
                        binsert(gc->ShaderMap->Key, 0, effectName, '?');
                    }

                    // Check for a version mapping.
                    if (gc->TokenMap)
                    {
                        struct bstrList* tokens = bsplit(shaderKey, '.');
                        glswList* pTokenMapping = gc->TokenMap;

                        while (pTokenMapping)
                        {
                            bstring directive = 0;
                            int tokenIndex;

                            // An empty key in the token mapping means "always prepend this directive".
                            // The effect name itself is also checked against the token mapping.
                            if (0 == blength(pTokenMapping->Key) ||
                                (1 == blength(pTokenMapping->Key) && '*' == bchar(pTokenMapping->Key, 0)) ||
                                1 == biseq(pTokenMapping->Key, effectName))
                            {
                                directive = pTokenMapping->Value;
                                binsert(gc->ShaderMap->Value, 0, directive, '?');
                            }

                            // Check all tokens in the current section divider for a mapped token.
                            for (tokenIndex = 0; tokenIndex < tokens->qty && !directive; tokenIndex++)
                            {
                                bstring token = tokens->entry[tokenIndex];
                                if (1 == biseq(pTokenMapping->Key, token))
                                {
                                    directive = pTokenMapping->Value;
                                    binsert(gc->ShaderMap->Value, 0, directive, '?');
                                }
                            }

                            pTokenMapping = pTokenMapping->Next;
                        }

                        bstrListDestroy(tokens);
                    }
                }

                continue;
            }
            if (shaderKey)
            {
                bconcat(gc->ShaderMap->Value, line);
                bconchar(gc->ShaderMap->Value, '\n');
            }
        }

        // Cleanup
        bstrListDestroy(lines);
        bdestroy(shaderKey);
    }

    // Find the longest matching shader key
    pShaderEntry = gc->ShaderMap;

    while (pShaderEntry)
    {
        if (binstr(effectKey, 0, pShaderEntry->Key) == 0 &&
            (!closestMatch || blength(pShaderEntry->Key) > blength(closestMatch->Key)))
        {
            closestMatch = pShaderEntry;
        }

        pShaderEntry = pShaderEntry->Next;
    }

    bstrListDestroy(tokens);
    bdestroy(effectKey);

    if (!closestMatch)
    {
        bdestroy(gc->ErrorMessage);
        gc->ErrorMessage = bformat("Could not find shader with key '%s'.", pEffectKey);
        return 0;
    }

    return (const char*) closestMatch->Value->data;
}

const char* glswGetError()
{
    glswContext* gc = __glsw__Context;

    if (!gc)
    {
        return "The glsw API has not been initialized.";
    }

    return (const char*) (gc->ErrorMessage ? gc->ErrorMessage->data : 0);
}

int glswAddDirective(const char* token, const char* directive)
{
    glswContext* gc = __glsw__Context;
    glswList* temp;

    if (!gc)
    {
        return 0;
    }

    temp = gc->TokenMap;
    gc->TokenMap = (glswList*) calloc(sizeof(glswList), 1);
    gc->TokenMap->Key = bfromcstr(token);
    gc->TokenMap->Value = bfromcstr(directive);
    gc->TokenMap->Next = temp;

    bconchar(gc->TokenMap->Value, '\n');

    return 1;
}
