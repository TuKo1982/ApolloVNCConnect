#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

/* Compatibilité GCC 2.95 */
#ifndef IPTR
typedef unsigned long IPTR;
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) (((ULONG)(a)<<24)|((ULONG)(b)<<16)|((ULONG)(c)<<8)|(ULONG)(d))
#endif

struct Library *MUIMasterBase = NULL;

#define ID_CONNECT 1001
#define ID_SAVE    1002

/* Liste des formats de couleurs pour le Cycle Gadget */
static const char *ColorFormats[] = {
    "YUV4", "YUV4N", "YUV5", "YUV6", "YUV7", "YUV8",
    "RGB8", "RGB9", "RGB12", "RGB16",
    NULL
};

/* Fonction utilitaire pour retrouver l'index (0..9) à partir du texte lu dans le fichier */
static ULONG GetFormatIndex(STRPTR fmtName) {
    int i = 0;
    if (!fmtName || !fmtName[0]) return 0; /* Par défaut YUV4 (index 0) */

        while (ColorFormats[i]) {
            if (strcmp(ColorFormats[i], fmtName) == 0) return i;
            i++;
        }
        return 0; /* Si non trouvé, retour à YUV4 */
}

/* Sauvegarde : On passe maintenant le format (chaine) au lieu du user */
static void SavePrefs(STRPTR host, STRPTR fmt, STRPTR pass) {
    BPTR fh = Open("ENVARC:ApolloVNC.prefs", MODE_NEWFILE);
    if (fh) {
        FPuts(fh, "SERVER=");   FPuts(fh, host); FPuts(fh, "\n");
        FPuts(fh, "PASSWORD="); FPuts(fh, pass); FPuts(fh, "\n");
        FPuts(fh, "COLORFMT="); FPuts(fh, fmt);  FPuts(fh, "\n"); /* Changement de clé */
        Close(fh);
    }
}

/* Chargement : On lit COLORFMT au lieu de USERNAME */
static void LoadPrefs(STRPTR host, STRPTR fmt, STRPTR pass) {
    BPTR fh = Open("ENVARC:ApolloVNC.prefs", MODE_OLDFILE);
    char line[256];

    /* Valeurs par défaut */
    host[0] = 0;
    fmt[0]  = 0; /* Sera converti en index 0 (YUV4) par GetFormatIndex si vide */
    pass[0] = 0;

    if (fh) {
        while (FGets(fh, line, 256)) {
            line[strcspn(line, "\r\n")] = 0;

            if (strncmp(line, "SERVER=", 7) == 0) {
                strcpy(host, line + 7);
            }
            else if (strncmp(line, "PASSWORD=", 9) == 0) {
                strcpy(pass, line + 9);
            }
            else if (strncmp(line, "COLORFMT=", 9) == 0) {
                strcpy(fmt, line + 9);
            }
        }
        Close(fh);
    }
}

int main(void) {
    /* Déclarations */
    Object *app, *win, *strHost, *cycFmt, *strPass;
    Object *btnConnect, *btnSave;

    char bufHost[128] = "", bufFmt[128] = "", bufPass[128] = "";
    char cmd[512];
    STRPTR h, p;
    ULONG idxFmt; /* Pour stocker l'index du cycle gadget */
    ULONG initFmtIdx = 0;

    ULONG signals = 0;
    ULONG res;
    BOOL running = TRUE;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;

    /* Chargement des prefs */
    LoadPrefs(bufHost, bufFmt, bufPass);
    /* Conversion du texte (ex: "RGB16") en index (ex: 9) pour l'initialisation du GUI */
    initFmtIdx = GetFormatIndex(bufFmt);

    app = ApplicationObject,
    MUIA_Application_Title,   (IPTR)"ApolloVNC Connect",
    MUIA_Application_Base,    (IPTR)"APVNC",

    SubWindow, win = WindowObject,
    MUIA_Window_Title, (IPTR)"ApolloVNC Connect",
    MUIA_Window_ID,    (IPTR)MAKE_ID('V','N','C','L'),
    MUIA_Window_RootObject, VGroup,
    MUIA_Background, MUII_GroupBack,

    Child, GroupObject,
    MUIA_Group_Columns, 2,

    Child, TextObject, MUIA_Text_Contents, (IPTR)"Host IP:", End,
    Child, strHost = StringObject, MUIA_String_Contents, (IPTR)bufHost, End,

    /* Remplacement du champ texte USER par un Cycle Gadget COLORFMT */
    Child, TextObject, MUIA_Text_Contents, (IPTR)"Color Format:", End,
    Child, cycFmt = CycleObject,
    MUIA_Cycle_Entries, (IPTR)ColorFormats,
    MUIA_Cycle_Active,  (IPTR)initFmtIdx,
    End,

    Child, TextObject, MUIA_Text_Contents, (IPTR)"Password:", End,
    Child, strPass = StringObject,
    MUIA_String_Contents, (IPTR)bufPass,
    MUIA_String_Secret, TRUE,
    End,
    End,

    Child, GroupObject,
    MUIA_Group_Horiz, TRUE,
    Child, btnConnect = MUI_MakeObject(MUIO_Button, (IPTR)"_Connect"),
    Child, btnSave    = MUI_MakeObject(MUIO_Button, (IPTR)"_Save"),
    End,
    End,
    End,
    End;

    if (!app) { CloseLibrary(MUIMasterBase); return 20; }

    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    DoMethod(btnConnect, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, ID_CONNECT);

    DoMethod(btnSave, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, ID_SAVE);

    set(win, MUIA_Window_Open, TRUE);

    while (running) {
        res = DoMethod(app, MUIM_Application_NewInput, (IPTR)&signals);

        if (res == MUIV_Application_ReturnID_Quit) {
            running = FALSE;
        }
        else if (res == ID_CONNECT) {
            get(strHost, MUIA_String_Contents, &h);
            get(strPass, MUIA_String_Contents, &p);

            /* Récupération de l'index sélectionné dans la liste déroulante */
            get(cycFmt, MUIA_Cycle_Active, &idxFmt);

            /* ApolloVNC IP USER PASS -> Ici USER est remplacé par COLORFMT */
            /* On utilise ColorFormats[idxFmt] pour récupérer la chaine (ex: "RGB16") */
            sprintf(cmd, "ApolloVNC %s %s %s", h, ColorFormats[idxFmt], p);

            Execute((STRPTR)cmd, 0, 0);
        }
        else if (res == ID_SAVE) {
            get(strHost, MUIA_String_Contents, &h);
            get(strPass, MUIA_String_Contents, &p);

            /* Récupération de l'index pour la sauvegarde */
            get(cycFmt, MUIA_Cycle_Active, &idxFmt);

            SavePrefs(h, (STRPTR)ColorFormats[idxFmt], p);
            DisplayBeep(NULL);
        }

        if (running && signals) {
            signals = Wait(signals | SIGBREAKF_CTRL_C);
            if (signals & SIGBREAKF_CTRL_C) running = FALSE;
        }
    }

    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
