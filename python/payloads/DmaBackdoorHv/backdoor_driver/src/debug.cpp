#include "stdafx.h"

#define DBG_BUFF_SIZE 0x100

#pragma intrinsic(strlen)

typedef int (__cdecl * func_sprintf)(char *s, const char *format, ...);
typedef int (__cdecl * func_vsprintf)(char *s, const char *format, va_list arg);
//--------------------------------------------------------------------------------------
#ifdef DBG
//--------------------------------------------------------------------------------------
char *DbgNameFromPath(char *lpszPath)
{
    char *lpszName = lpszPath;

    for (size_t i = 0; i < strlen(lpszPath); i++)
    {
        if (lpszPath[i] == '\\' || lpszPath[i] == '/')
        {
            lpszName = lpszPath + i + 1;
        }
    }

    return lpszName;
}
//--------------------------------------------------------------------------------------
void DbgMsg(char *lpszFile, int Line, char *lpszMsg, ...)
{    
    func_sprintf f_sprintf = (func_sprintf)ImportGetProcAddress(0, h_sprintf);
    func_vsprintf f_vsprintf = (func_vsprintf)ImportGetProcAddress(0, h_vsprintf);

    if (f_sprintf && f_vsprintf)
    {
        va_list arg;
        char szBuff[DBG_BUFF_SIZE], szOutBuff[DBG_BUFF_SIZE];

        va_start(arg, lpszMsg);
        f_vsprintf(szBuff, lpszMsg, arg);   
        va_end(arg);

        f_sprintf(szOutBuff, "%s(%d) : %s", DbgNameFromPath(lpszFile), Line, szBuff);   

        GDbgPrint(szOutBuff);
    }    
}
//--------------------------------------------------------------------------------------
#endif // DBG
//--------------------------------------------------------------------------------------
// EoF
