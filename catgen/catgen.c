/*
 * Copyright 2020, HP Inc.
 * Author: Christian Limpach <Christian.Limpach@gmail.com>
 * SPDX-License-Identifier: ISC
 */

#include <windows.h>
#include <config.h>
#include <stdarg.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <stdint.h>
#include <getopt.h>

#include <setupapi.h>

BOOL CreateCatEx(LPCSTR szCatPath, LPCSTR szHWID, LPCSTR szSearchDir, LPCSTR* szFileList, DWORD cFileList, LPCWSTR wszOS, LPCWSTR wszOSAttr);

static wchar_t *_utf8_to_wide(const char *s);
static void errx_msg(const char *fmt, ...);
static void err_msg(const char *fmt, ...);
static void verbose_msg(const char *fmt, ...);

static int parse_inf(char *inf_name, const char **cat_list,
		     unsigned int *cat_list_idx, char **hw_id);

#define DEFAULT_OS L"7X64,8X64,10X64"
#define DEFAULT_OSATTR L"2:6.1,2:6.2,2:6.4"
#define EXAMPLE_HWID "PNP0F13"

#define CAT_LIST_MAX_ENTRIES 128

static int verbose = 0;

static void
usage(const char *progname)
{
    fprintf(stderr, "usage: %s [options]... [file]...\n\n", progname);
    fprintf(stderr, "  -o, --out\n	output cat file\n");
    fprintf(stderr, "  -d, --drv-path\n	dir containing files\n");
    fprintf(stderr, "  -i, --inf-file\n	parse inf file\n");
    fprintf(stderr, "  -h, --hwid\n	hwid (example: %s)\n", EXAMPLE_HWID);
    fprintf(stderr, "  -O, --OS\n	OS string (default: %S)\n", DEFAULT_OS);
    fprintf(stderr, "  -A, --OSAttr\n	OSAttr string (default: %S)\n", DEFAULT_OSATTR);
    exit(1);
}

int
main(int argc, char **argv)
{
    int ret;
    char *cat_path = NULL, *drv_path = NULL, *inf_file = NULL, *hw_id = NULL;
    const char *cat_list[CAT_LIST_MAX_ENTRIES+1];
    unsigned int nb_entries = 0;
    LPCWSTR wszOS = DEFAULT_OS;
    LPCWSTR wszOSAttr = DEFAULT_OSATTR;

    while (1) {
        int c, index = 0;

        enum { LI_ };

        static int long_index;
        static struct option long_options[] = {
            {"out",           required_argument, NULL,       'o'},
            {"drv-path",      required_argument, NULL,       'd'},
            {"inf-file",      required_argument, NULL,       'i'},
            {"hwid",          required_argument, NULL,       'h'},
            {"OS",            required_argument, NULL,       'O'},
            {"OSAttr",        required_argument, NULL,       'A'},
            {"verbose",       no_argument,       NULL,       'v'},
            {NULL,   0,                 NULL, 0}
        };

        long_index = 0;
        c = getopt_long(argc, argv, "o:d:i:h:O:A:v", long_options,
                        &index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            switch (long_index) {
            case LI_:
                break;
            }
            break;
        case 'o':
	    cat_path = optarg;
	    break;
        case 'd':
	    drv_path = optarg;
	    break;
        case 'i':
	    inf_file = optarg;
	    break;
        case 'h':
	    hw_id = optarg;
	    break;
        case 'O':
	    wszOS = _utf8_to_wide(optarg);
	    break;
        case 'A':
	    wszOSAttr = _utf8_to_wide(optarg);
	    break;
	case 'v':
	    verbose = 1;
	    break;
        }
    }

    if (!drv_path || !cat_path)
	usage(argv[0]);

    if (inf_file) {
	char *p = NULL;

	if (inf_file[0] != '/' && inf_file[0] != '\\') {
	    cat_list[nb_entries++] = inf_file;
	    asprintf(&p, "%s/%s", drv_path, inf_file);
	} else {
	    cat_list[nb_entries] = strrchr(inf_file, '/');
	    if (cat_list[nb_entries]) {
		cat_list[nb_entries]++;
		nb_entries++;
		asprintf(&p, "%s", inf_file);
	    }
	}

	ret = parse_inf(p, cat_list, &nb_entries, &hw_id);
	if (!ret) {
	    errx_msg("parse inf failed");
	    return 1;
	}
    }

    while (optind < argc) {
	char *f = strrchr(argv[optind], '/');
	if (f)
	    f++;
	else
	    f= argv[optind];
	cat_list[nb_entries++] = f;
	optind++;
    }

    verbose_msg("hw_id %s", hw_id);
    for (unsigned int i = 0; i < nb_entries; i++)
	verbose_msg("cat_list[%d] = %s", i, cat_list[i]);

    ret = CreateCatEx(cat_path, hw_id, drv_path, cat_list, nb_entries,
		      wszOS, wszOSAttr);
    if (!ret) {
	errx_msg("CreateCat failed");
	return 1;
    }

    return 0;
}

static wchar_t *
_utf8_to_wide(const char *s)
{
    int sz;
    wchar_t *ws;

    /* First figure out buffer size needed and malloc it. */
    sz = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (!sz)
        return NULL;

    ws = (wchar_t *)malloc(sizeof(wchar_t) * (sz + 1));
    if (!ws)
        return NULL;
    ws[sz] = 0;

    /* Now perform the actual conversion. */
    sz = MultiByteToWideChar(CP_UTF8, 0, s, -1, ws, sz);
    if (!sz) {
        free(ws);
        ws = NULL;
    }

    return ws;
}

static inline char *
_wide_to_utf8(const wchar_t *ws)
{
    int sz;
    char *s;

    /* First figure out buffer size needed and malloc it. */
    sz = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, 0);
    if (!sz)
        return NULL;

    s = (char *)malloc(sz + sizeof(char));
    if (s == NULL)
        return NULL;
    s[sz] = 0;

    /* Now perform the actual conversion. */
    sz = WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, sz, NULL, 0);
    if (!sz) {
        free(s);
        s = NULL;
    }

    return s;
}

static void
errx_msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}

static void
err_msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, ": failed %lx\n", GetLastError());
}

static void
verbose_msg(const char *fmt, ...)
{
    va_list ap;

    if (!verbose)
	return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}

static int
parse_inf(char *inf_name, const char **cat_list, unsigned int *cat_list_idx,
	  char **hw_id)
{
    BOOL ret;
    WCHAR bufW[MAX_PATH];
    HINF hinf;
    int rc = 0;

    hinf = SetupOpenInfFileA(inf_name, NULL, INF_STYLE_WIN4, NULL);
    if (hinf == INVALID_HANDLE_VALUE) {
	err_msg("SetupOpenInfFileA(%s)", inf_name);
	goto out;
    }

    LPCWSTR WINAPI pSetupGetField(PINFCONTEXT context, DWORD index);

    SetLastError(0xdeadbeef);
    lstrcmpW(NULL, NULL);
    if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
	errx_msg("oh no, no unicode!?");
	exit (1);
    }

    INFCONTEXT c_man;
    ret = SetupFindFirstLineW(hinf, L"Manufacturer", NULL, &c_man);
    if (!ret) {
	errx_msg("empty Manufacturer section");
	goto out;
    }

    do {
	int fc = SetupGetFieldCount(&c_man);
	int f = 1;

	const WCHAR *models_section_name = pSetupGetField(&c_man, f);
	verbose_msg("models section name %S", models_section_name);
	if (f < fc)
	    f++;

	for (; f <= fc; f++) {
	    WCHAR model[LINE_LEN * 2 + 1 + 1];
	    const WCHAR *targetOSVersion = NULL;
	    if (f != 1) {
		targetOSVersion = pSetupGetField(&c_man, f);
		verbose_msg("targetOSVersion %S", targetOSVersion);
	    }

	    model[0] = 0;
	    wcsncat_s(model, sizeof(model), models_section_name, _TRUNCATE);
	    if (targetOSVersion) {
		wcsncat_s(model, sizeof(model), L".", _TRUNCATE);
		wcsncat_s(model, sizeof(model), targetOSVersion, _TRUNCATE);
	    }

	    verbose_msg("model %S", model);

	    INFCONTEXT c_desc;
	    ret = SetupFindFirstLineW(hinf, model, NULL, &c_desc);
	    if (!ret)
		continue;

	    do {
		verbose_msg("  desc 0 %S", pSetupGetField(&c_desc, 0));
		verbose_msg("  desc 1 %S", pSetupGetField(&c_desc, 1));
		verbose_msg("  desc 2 %S", pSetupGetField(&c_desc, 2));

		if (SetupGetFieldCount(&c_desc) >= 2 && ((*hw_id) == NULL)) {
		    *hw_id = _wide_to_utf8(pSetupGetField(&c_desc, 2));
		    if ((*hw_id)[0] == '*')
			(*hw_id)++;
		}

		const WCHAR *inst = pSetupGetField(&c_desc, 1);

		for (unsigned int sec = 0; ; sec++) {
		    ret = SetupEnumInfSectionsW(hinf, sec, bufW, sizeof(bufW),
						NULL);
		    if (!ret) {
			if (GetLastError() == ERROR_NO_MORE_ITEMS)
			    break;
			err_msg("SetupEnumInfSectionsA");
			continue;
		    }
		    verbose_msg("found section %S", bufW);

		    if (wcsncmp(bufW, inst, wcslen(inst)))
			continue;
		    if (bufW[wcslen(inst)] && bufW[wcslen(inst)] != '.')
			continue;
		    verbose_msg("      install section %S", bufW);

		    INFCONTEXT c_copy;
		    ret = SetupFindFirstLineW(hinf, bufW, L"CopyFiles",
					      &c_copy);
		    if (!ret)
			continue;
		    verbose_msg("      CopyFiles in section %S", bufW);

		    do {
			const WCHAR *copy = pSetupGetField(&c_copy, 1);

			verbose_msg("sec %S copy file %S", bufW, copy);

			if (copy[0] == '@') {
			    cat_list[*cat_list_idx] = _wide_to_utf8(&copy[1]);
			    verbose_msg("cat_list[%d] = %s", *cat_list_idx,
					cat_list[*cat_list_idx]);
			    (*cat_list_idx)++;
			} else {
			    INFCONTEXT c_files;
			    ret = SetupFindFirstLineW(hinf, copy, NULL,
						      &c_files);
			    if (!ret)
				continue;
			    do {
				cat_list[*cat_list_idx] =
				    _wide_to_utf8(pSetupGetField(&c_files, 1));
				verbose_msg("cat_list[%d] = %s", *cat_list_idx,
					    cat_list[*cat_list_idx]);
				(*cat_list_idx)++;
			    } while (SetupFindNextMatchLineW(&c_files, NULL,
							     &c_files));
			}
		    } while (SetupFindNextMatchLineW(&c_copy, L"CopyFiles",
						     &c_copy));
		}
	    } while (SetupFindNextMatchLineW(&c_desc, NULL, &c_desc));
	}
    } while (SetupFindNextMatchLineW(&c_man, NULL, &c_man));

    rc = 1;
  out:
    if (hinf != INVALID_HANDLE_VALUE)
	SetupCloseInfFile(hinf);

    return rc;
}
