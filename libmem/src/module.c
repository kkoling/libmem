#include "internal.h"

LM_API lm_bool_t
LM_EnumModules(lm_bool_t(*callback)(lm_module_t  mod,
				    lm_tstring_t path,
				    lm_void_t   *arg),
	       lm_void_t *arg)
{
	LM_ASSERT(callback != LM_NULLPTR);

	return LM_EnumModulesEx(LM_GetProcessId(), callback, arg);
}

/********************************/

#if LM_OS == LM_OS_WIN
LM_PRIVATE lm_bool_t
_LM_EnumModulesEx(lm_pid_t   pid,
		  lm_bool_t(*callback)(lm_module_t  mod,
				       lm_tstring_t path,
				       lm_void_t   *arg),
		  lm_void_t *arg)
{
	lm_bool_t ret = LM_FALSE;
	HANDLE hSnap;
	MODULEENTRY32 entry;

	hSnap = CreateToolhelp32Snapshot(
		TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
		pid
	);

	if (hSnap == INVALID_HANDLE_VALUE)
		return ret;

	entry.dwSize = sizeof(MODULEENTRY32);

	if (Module32First(hSnap, &entry)) {
		do {
			lm_module_t mod;

			mod.base = (lm_address_t)(
				entry.modBaseAddr
			);
			mod.size = (lm_size_t)(
				entry.modBaseSize
			);
			mod.end  = (lm_address_t)(
				&((lm_byte_t *)mod.base)[mod.size]
			);

			if (callback(mod, entry.szExePath, arg) == LM_FALSE)
				break;
		} while (Module32Next(hSnap, &entry));

		ret = LM_TRUE;
	}

	CloseHandle(hSnap);

	return ret;
}
#else
LM_PRIVATE lm_bool_t
_LM_EnumModulesEx(lm_pid_t   pid,
		  lm_bool_t(*callback)(lm_module_t  mod,
				       lm_tstring_t path,
				       lm_void_t   *arg),
		  lm_void_t *arg)
{
	lm_bool_t    ret = LM_FALSE;
	lm_tchar_t   maps_path[LM_PATH_MAX];
	FILE        *maps_file;
	lm_tchar_t  *maps_line = NULL;
	lm_size_t    maps_line_len;
	ssize_t      line_len;
	regex_t      regex;
	regmatch_t   matches[5];
	lm_module_t  mod;
	lm_tchar_t   path[LM_PATH_MAX] = { 0 };
	lm_tstring_t curpath;

#	if LM_OS == LM_OS_BSD
	if (regcomp(&regex, "^0x([a-z0-9]+)[[:blank:]]+0x([a-z0-9]+)[[:blank:]]+[^/]+(/.*)([[:blank:]])+[A-Z]+[[:blank:]]+.*$", REG_EXTENDED))
		return ret;

	LM_SNPRINTF(maps_path, LM_ARRLEN(maps_path),
		    LM_STR("%s/%d/map"), LM_PROCFS, pid);
#	else
	if (regcomp(&regex, "^([a-z0-9]+)-([a-z0-9]+)[^/]+(/.+)$", REG_EXTENDED))
		return ret;

	LM_SNPRINTF(maps_path, LM_ARRLEN(maps_path),
		    LM_STR("%s/%d/maps"), LM_PROCFS, pid);
#	endif

	maps_file = LM_FOPEN(maps_path, "r");
	if (!maps_file)
		goto FREE_EXIT;

	while ((line_len = LM_GETLINE(&maps_line, &maps_line_len, maps_file)) > 0) {
		if (regexec(&regex, maps_line, LM_ARRLEN(matches), matches, 0))
			continue;

		maps_line[--line_len] = LM_STR('\x00'); /* remove \n */
#		if LM_OS == LM_OS_BSD
		maps_line[matches[4].rm_so] = LM_STR('\x00');
#		endif
		curpath = &maps_line[matches[3].rm_so];


		/* TODO: Group copies of base and path of first and new module conditions */

		/* if it is the first module, copy the base and path */
		if (LM_STRLEN(path) == 0) {
			lm_size_t pathlen = LM_STRLEN(curpath);

			if (pathlen >= LM_ARRLEN(path))
				pathlen = LM_ARRLEN(path) - 1;

			LM_STRNCPY(path, curpath, pathlen);
			path[pathlen] = LM_STR('\x00');

			mod.base = (lm_address_t)LM_STRTOP(
				&maps_line[matches[1].rm_so], NULL, 16
			);
		}

		/* if the module changes, run a callback and copy the new base and path */
		if (LM_STRCMP(curpath, path)) {
			lm_size_t pathlen;

			mod.size = (lm_size_t)(
				(lm_uintptr_t)mod.end - (lm_uintptr_t)mod.base
			);

			if (callback(mod, path, arg) == LM_FALSE)
				break;

			pathlen = LM_STRLEN(curpath);
			if (pathlen >= LM_ARRLEN(path))
				pathlen = LM_ARRLEN(path) - 1;

			LM_STRNCPY(path, curpath, pathlen);
			path[pathlen] = LM_STR('\x00');

			mod.base = (lm_address_t)LM_STRTOP(
				&maps_line[matches[1].rm_so], NULL, 16
			);
		}

		/* the module end address should always update, since it's supposed
		   to be the last valid address for a module */
		mod.end = (lm_address_t)LM_STRTOP(
			&maps_line[matches[2].rm_so], NULL, 16
		);
	}


	ret = LM_TRUE;

	LM_FCLOSE(maps_file);
FREE_EXIT:
	regfree(&regex);
	return ret;
}
#endif

LM_API lm_bool_t
LM_EnumModulesEx(lm_pid_t   pid,
		 lm_bool_t(*callback)(lm_module_t  mod,
				      lm_tstring_t path,
				      lm_void_t   *arg),
		 lm_void_t *arg)
{
	LM_ASSERT(pid != LM_PID_BAD && callback != LM_NULLPTR);

	return _LM_EnumModulesEx(pid, callback, arg);
}

/********************************/

typedef struct {
	lm_module_t *modbuf;
	lm_tstring_t name;
	lm_size_t    len;
} _lm_find_mod_t;

LM_PRIVATE lm_bool_t
_LM_FindModuleCallback(lm_module_t  mod,
		       lm_tstring_t path,
		       lm_void_t   *arg)
{
	_lm_find_mod_t *parg = (_lm_find_mod_t *)arg;
	lm_size_t       pathlen;

	pathlen = LM_STRLEN(path);

	if (pathlen >= parg->len) {
		if (!LM_STRCMP(&path[pathlen - parg->len], parg->name)) {
			*(parg->modbuf) = mod;
			return LM_FALSE;
		}
	}

	return LM_TRUE;
}

LM_API lm_bool_t
LM_FindModule(lm_tstring_t name,
	      lm_module_t *modbuf)
{
	_lm_find_mod_t arg;

	LM_ASSERT(name != LM_NULLPTR && modbuf != LM_NULLPTR);

	arg.modbuf = modbuf;
	arg.modbuf->size = 0;
	arg.name = name;
	arg.len = LM_STRLEN(arg.name);

	if (!LM_EnumModules(_LM_FindModuleCallback, (lm_void_t *)&arg))
		return LM_FALSE;

	return arg.modbuf->size > 0 ? LM_TRUE : LM_FALSE;
}

/********************************/

LM_API lm_bool_t
LM_FindModuleEx(lm_pid_t     pid,
		lm_tstring_t name,
		lm_module_t *modbuf)
{
	_lm_find_mod_t arg;

	LM_ASSERT(pid != LM_PID_BAD &&
		  name != LM_NULLPTR &&
		  modbuf != LM_NULLPTR);

	arg.modbuf = modbuf;
	arg.modbuf->size = 0;
	arg.name = name;
	arg.len = LM_STRLEN(arg.name);

	if (!LM_EnumModulesEx(pid, _LM_FindModuleCallback, (lm_void_t *)&arg))
		return LM_FALSE;

	return arg.modbuf->size > 0 ? LM_TRUE : LM_FALSE;
}

/********************************/

typedef struct {
	lm_module_t  mod;
	lm_tstring_t pathbuf;
	lm_size_t    maxlen;
	lm_size_t    len;
} _lm_get_mod_path_t;

LM_PRIVATE lm_bool_t
_LM_GetModulePathCallback(lm_module_t  mod,
			  lm_tstring_t path,
			  lm_void_t   *arg)
{
	_lm_get_mod_path_t *parg = (_lm_get_mod_path_t *)arg;
	
	if (parg->mod.base == mod.base) {
		parg->len = LM_STRLEN(path);
		if (parg->len >= parg->maxlen)
			parg->len = parg->maxlen - 1;
		LM_STRNCPY(parg->pathbuf, path, parg->len);
		parg->pathbuf[parg->len] = LM_STR('\x00');
	}

	return LM_TRUE;
}

LM_API lm_size_t
LM_GetModulePath(lm_module_t mod,
		 lm_tchar_t *pathbuf,
		 lm_size_t   maxlen)
{
	_lm_get_mod_path_t arg;

	LM_ASSERT(pathbuf != LM_NULLPTR && maxlen > 0);

	arg.mod     = mod;
	arg.pathbuf = pathbuf;
	arg.maxlen  = maxlen;
	arg.len     = 0;

	LM_EnumModules(_LM_GetModulePathCallback, (lm_void_t *)&arg);

	return arg.len;
}

/********************************/

LM_API lm_size_t
LM_GetModulePathEx(lm_pid_t    pid,
		   lm_module_t mod,
		   lm_tchar_t *pathbuf,
		   lm_size_t   maxlen)
{
	_lm_get_mod_path_t arg;

	LM_ASSERT(pid != LM_PID_BAD && pathbuf != LM_NULLPTR && maxlen > 0);

	arg.mod     = mod;
	arg.pathbuf = pathbuf;
	arg.maxlen  = maxlen;
	arg.len     = 0;

	LM_EnumModulesEx(pid, _LM_GetModulePathCallback, (lm_void_t *)&arg);

	return arg.len;
}

/********************************/

LM_API lm_size_t
LM_GetModuleName(lm_module_t mod,
		 lm_tchar_t *namebuf,
		 lm_size_t   maxlen)
{
	lm_size_t   len = 0;
	lm_tchar_t  path[LM_PATH_MAX];
	lm_tchar_t *holder;

	LM_ASSERT(namebuf != LM_NULLPTR && maxlen > 0);

	if (!LM_GetModulePath(mod, path, LM_PATH_MAX))
		return len;

	holder = LM_STRRCHR(path, LM_PATH_SEP);
	holder = &holder[1]; /* don't include the path separator */

	len = LM_STRLEN(holder);
	if (len >= maxlen)
		len = maxlen - 1;

	LM_STRNCPY(namebuf, holder, len);
	namebuf[len] = LM_STR('\x00');

	return len;
}

/********************************/

LM_API lm_size_t
LM_GetModuleNameEx(lm_pid_t    pid,
		   lm_module_t mod,
		   lm_tchar_t *namebuf,
		   lm_size_t   maxlen)
{
	lm_size_t   len = 0;
	lm_tchar_t  path[LM_PATH_MAX];
	lm_tchar_t *holder;

	LM_ASSERT(pid != LM_PID_BAD &&
		  namebuf != LM_NULLPTR &&
		  maxlen > 0);

	if (!LM_GetModulePathEx(pid, mod, path, LM_PATH_MAX))
		return len;

	holder = LM_STRRCHR(path, LM_PATH_SEP);
	holder = &holder[1]; /* don't include the path separator */

	len = LM_STRLEN(holder);
	if (len >= maxlen)
		len = maxlen - 1;

	LM_STRNCPY(namebuf, holder, len);
	namebuf[len] = LM_STR('\x00');

	return len;
}

/********************************/

#if LM_OS == LM_OS_WIN
LM_PRIVATE lm_bool_t
_LM_LoadModule(lm_tstring_t path)
{
	return LoadLibrary(path) ? LM_TRUE : LM_FALSE;
}
#else
LM_PRIVATE lm_bool_t
_LM_LoadModule(lm_tstring_t path)
{
	return dlopen(path, RTLD_LAZY) ? LM_TRUE : LM_FALSE;
}
#endif

LM_API lm_bool_t
LM_LoadModule(lm_tstring_t path,
	      lm_module_t *modbuf)
{
	/* modbuf can be NULL. in that case, the module info won't be saved */
	LM_ASSERT(path != LM_NULLPTR);

	if (!_LM_LoadModule(path))
		return LM_FALSE;

	/* TODO (?): Unload the module if it doesn't find it */
	if (modbuf && !LM_FindModule(path, modbuf))
		return LM_FALSE;

	return LM_TRUE;
}

/********************************/

#if LM_OS == LM_OS_WIN
LM_PRIVATE lm_bool_t
_LM_LoadModuleEx(lm_pid_t     pid,
		 lm_tstring_t path)
{
	lm_bool_t    ret = LM_FALSE;
	HANDLE       hProcess;
	lm_size_t    modpath_size;
	lm_address_t modpath_addr;
	HANDLE       hThread;

	if (!_LM_OpenProcess(&hProcess))
		return ret;

	modpath_size = (LM_STRLEN(path) + 1) * sizeof(lm_tchar_t)
	modpath_addr = LM_AllocMemoryEx(pid, modpath_size, LM_PROT_XRW);
	if (modpath_addr == LM_ADDRESS_BAD)
		goto CLOSE_EXIT;

	if (!LM_WriteMemoryEx(hProcess, modpath_addr, path, modpath_size))
		goto FREE_EXIT;

	hThread = (HANDLE)CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, modpath_addr, 0, NULL);
	if (!hThread)
		goto FREE_RET;

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);

	ret = LM_TRUE;
FREE_EXIT:
	LM_FreeMemoryEx(pid, modpath_addr, modpath_size);
CLOSE_EXIT:
	_LM_CloseProcess(&hProcess);
	return ret;
}
#else
LM_PRIVATE lm_bool_t
_LM_LoadModuleEx(lm_pid_t     pid,
		 lm_tstring_t path)
{
	if (!_LM_CallDlopen(pid, path, RTLD_LAZY, LM_NULLPTR))
		return LM_FALSE;

	

	return LM_TRUE;
}
#endif

LM_API lm_bool_t
LM_LoadModuleEx(lm_pid_t     pid,
		lm_tstring_t path,
		lm_module_t *modbuf)
{
	LM_ASSERT(pid != LM_PID_BAD && path != LM_NULLPTR);

	if (!_LM_LoadModuleEx(pid, path))
		return LM_FALSE;

	/* TODO (?): Unload module if it is not found */
	if (modbuf && !LM_FindModuleEx(pid, path, modbuf))
		return LM_FALSE;

	return LM_TRUE;
}

/********************************/

#if LM_OS == LM_OS_WIN
LM_PRIVATE lm_bool_t
_LM_UnloadModule(lm_module_t mod)
{
	HMODULE hModule;

	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
			  (LPTSTR)mod.base, &hModule);

	if (!hModule)
		return LM_FALSE:

	FreeLibrary(hModule);
	return LM_TRUE;
}
#else
LM_PRIVATE lm_bool_t
_LM_UnloadModule(lm_module_t mod)
{
	lm_tchar_t libpath[LM_PATH_MAX];
	void *libhandle;

	if (!LM_GetModulePath(mod, libpath, LM_PATH_MAX))
		return LM_FALSE;

	/* reopen the library without loading, which gives us the
	   handle that we can use to decrease the reference count
	   and unload the library */
	libhandle = dlopen(libpath, RTLD_NOLOAD);

	if (!libhandle)
		return LM_FALSE;

	dlclose(libhandle);
	dlclose(libhandle);

	return LM_TRUE;
}
#endif

LM_API lm_bool_t
LM_UnloadModule(lm_module_t mod)
{
	return _LM_UnloadModule(mod);
}

/********************************/

#if LM_OS == LM_OS_WIN
LM_PRIVATE lm_bool_t
_LM_UnloadModuleEx(lm_pid_t    pid,
		   lm_module_t mod)
{
	lm_bool_t ret = LM_FALSE;
	HANDLE hProcess;
	HANDLE hSnap;
	MODULEENTRY32 entry;
	HMODULE hModule = NULL;
	HANDLE hThread;

	if (!_LM_OpenProcess(&hProcess))
		return ret;

	hSnap = CreateToolhelp32Snapshot(
		TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
		pid
	);

	if (hSnap == INVALID_HANDLE_VALUE)
		goto CLOSE_RET;

	entry.dwSize = sizeof(MODULEENTRY32);

	if (Module32First(hSnap, &entry)) {
		do {
			lm_module_t mod;
			if ((lm_address_t)entry.modBaseAddr == mod.base) {
				hModule = entry.hModule;
				break;
			}
		} while (Module32Next(hSnap, &entry));
	}

	CloseHandle(hSnap);

	if (!hModule)
		goto CLOSE_RET;

	hThread = (HANDLE)CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, hModule, 0, NULL);
	if (!hThread)
		goto CLOSE_RET;

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);

	ret = LM_TRUE;
CLOSE_RET:
	_LM_CloseProcess(&hProcess);
	return ret;
}
#else
LM_PRIVATE lm_bool_t
_LM_UnloadModuleEx(lm_pid_t    pid,
		   lm_module_t mod)
{
	lm_bool_t ret = LM_FALSE;
	void *modhandle;
	lm_tchar_t modpath[LM_PATH_MAX];

	if (!LM_GetModulePathEx(pid, mod, modpath, LM_PATH_MAX))
		return ret;

	if (!_LM_CallDlopen(pid, modpath, RTLD_NOLOAD, &modhandle))
		return ret;

	if (_LM_CallDlclose(pid, modhandle) && _LM_CallDlclose(pid, modhandle))
		ret = LM_TRUE;

	return ret;
}
#endif

LM_API lm_bool_t
LM_UnloadModuleEx(lm_pid_t    pid,
		  lm_module_t mod)
{
	return _LM_UnloadModuleEx(pid, mod);
}

