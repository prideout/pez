
int glswInit();
int glswShutdown();
int glswAddPath(const char* pathPrefix, const char* pathSuffix);
const char* glswGetShader(const char* effectKey);
const char* glswGetError();
int glswAddDirective(const char* token, const char* directive);

