#include <stddef.h>

int Pc_Ra_PreviewFirst(void) { return 0; }

int Pc_Ra_Why(const char* filter)
{
    (void)filter;
    return 0;
}

/* The console's system language, as the code of the language pack that fits it, or NULL when
 * the game should keep following the disc (English, or a language the game has no text for).
 * Ids are SCE_SYSTEM_PARAM_LANG_*; Spain and Latin America share Spanish, and so on. */
int sceSystemServiceParamGetInt(int id, int* value);

int g_Ps5SystemLangId = -1;

const char* ps5_system_language_code(void)
{
    int lang = -1;

    if (sceSystemServiceParamGetInt(1, &lang) != 0)
        return NULL;
    g_Ps5SystemLangId = lang;
    switch (lang)
    {
    case 0:  return "ja";
    case 2:  case 22: return "fr"; /* French, Canadian French */
    case 3:  case 20: return "es"; /* Spanish, Latin American Spanish */
    case 4:  return "de";
    case 5:  return "it";
    case 7:  case 17: return "pt"; /* Portuguese, Brazilian Portuguese */
    case 8:  return "ru";
    case 10: case 11: return "zh"; /* Traditional, Simplified Chinese */
    case 16: return "pl";
    case 18: return "en_pal";      /* UK English */
    default: return NULL;          /* US English and everything else: the disc's own */
    }
}

#include <stddef.h>

const size_t ps5_opengl_heap_size = 1536u * 1024u * 1024u;
