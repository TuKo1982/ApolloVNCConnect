#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

/* --- Compatibilité GCC 2.95 & SDK AmigaOS 3.x --- */
#ifndef IPTR
typedef unsigned long IPTR;
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) (((ULONG)(a)<<24)|((ULONG)(b)<<16)|((ULONG)(c)<<8)|(ULONG)(d))
#endif

/* --- CHAÎNE DE VERSION (Requis pour la commande CLI "Version") --- */
/* Format: $VER: <Nom> <Ver>.<Rev> (<Date>) */
const char verTag[] = "$VER: ApolloVNCConnect 1.0 (31.01.26)";

/* --- Variables Globales et Constantes --- */
struct Library *MUIMasterBase = NULL;

#define ID_CONNECT 1001
#define ID_SAVE    1002

/* Liste des formats pour la liste déroulante */
static const char *ColorFormats[] = {
    "YUV4", "YUV4N", "YUV5", "YUV6", "YUV7", "YUV8",
    "RGB8", "RGB9", "RGB12", "RGB16",
    NULL
};

/* --- Fonctions Utilitaires --- */

static ULONG GetFormatIndex(STRPTR fmtName) {
    int i = 0;
    if (!fmtName || !fmtName[0]) return 0; /* Défaut : YUV4 */
        while (ColorFormats[i]) {
            if (strcmp(ColorFormats[i], fmtName) == 0) return i;
            i++;
        }
        return 0;
}

static void SavePrefs(STRPTR host, STRPTR fmt, STRPTR pass) {
    BPTR fh = Open("ENVARC:ApolloVNC.prefs", MODE_NEWFILE);
    if (fh) {
        FPuts(fh, "SERVER=");   FPuts(fh, host); FPuts(fh, "\n");
        FPuts(fh, "PASSWORD="); FPuts(fh, pass); FPuts(fh, "\n");
        FPuts(fh, "COLORFMT="); FPuts(fh, fmt);  FPuts(fh, "\n");
        Close(fh);
    }
}

static void LoadPrefs(STRPTR host, STRPTR fmt, STRPTR pass) {
    BPTR fh = Open("ENVARC:ApolloVNC.prefs", MODE_OLDFILE);
    char line[256];

    /* Valeurs par défaut */
    host[0] = 0;
    fmt[0] = 0;
    pass[0] = 0;

    if (fh) {
        while (FGets(fh, line, 256)) {
            line[strcspn(line, "\r\n")] = 0;

            if (strncmp(line, "SERVER=", 7) == 0) strcpy(host, line + 7);
            else if (strncmp(line, "PASSWORD=", 9) == 0) strcpy(pass, line + 9);
            else if (strncmp(line, "COLORFMT=", 9) == 0) strcpy(fmt, line + 9);
        }
        Close(fh);
    }
}

/* --- Programme Principal --- */
int main(void) {
    Object *app, *win, *strHost, *cycFmt, *strPass;
    Object *btnConnect, *btnSave;

    char bufHost[128] = "", bufFmt[128] = "", bufPass[128] = "";
    char cmd[512];
    STRPTR h, p;
    ULONG idxFmt, initFmtIdx = 0;

    ULONG signals = 0;
    ULONG res;
    BOOL running = TRUE;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;

    LoadPrefs(bufHost, bufFmt, bufPass);
    initFmtIdx = GetFormatIndex(bufFmt);

    app = ApplicationObject,
    MUIA_Application_Title,   (IPTR)"ApolloVNC Launcher",
    MUIA_Application_Base,    (IPTR)"APVNC",
    /* Ajout de la version pour MUI et pour forcer l'inclusion de la chaine dans le binaire */
    MUIA_Application_Version, (IPTR)&verTag[1], /* On saute le '$' pour l'affichage MUI */

    SubWindow, win = WindowObject,
    MUIA_Window_Title, (IPTR)"ApolloVNC Connect",
    MUIA_Window_ID,    (IPTR)MAKE_ID('V','N','C','L'),

    /* GROUPE RACINE VERTICAL */
    MUIA_Window_RootObject, VGroup,
    MUIA_Group_HorizSpacing, 6,
    MUIA_Group_VertSpacing,  6,

    /* --- ZONE 1 : REGLAGES AVEC CADRE --- */
    Child, VGroup,
    MUIA_Frame, MUIV_Frame_Group,
    MUIA_FrameTitle, (IPTR)"Connection Settings",
    MUIA_Background, MUII_GroupBack,

    Child, GroupObject,
    MUIA_Group_Columns, 2,

    /* Ligne 1 : Host */
    Child, TextObject,
    MUIA_Text_Contents, (IPTR)"Host IP:",
    MUIA_Text_PreParse, (IPTR)"\33r",
    End,
    Child, strHost = StringObject,
    MUIA_Frame, MUIV_Frame_String,
    MUIA_String_Contents, (IPTR)bufHost,
    MUIA_String_AdvanceOnCR, TRUE,
    End,

    /* Ligne 2 : Password (déplacé ici) */
    Child, TextObject,
    MUIA_Text_Contents, (IPTR)"Password:",
    MUIA_Text_PreParse, (IPTR)"\33r",
    End,
    Child, strPass = StringObject,
    MUIA_Frame, MUIV_Frame_String,
    MUIA_String_Contents, (IPTR)bufPass,
    MUIA_String_Secret, TRUE,
    MUIA_String_AdvanceOnCR, TRUE,
    End,

    /* Ligne 3 : Format */
    Child, TextObject,
    MUIA_Text_Contents, (IPTR)"Format:",
    MUIA_Text_PreParse, (IPTR)"\33r",
    End,
    Child, cycFmt = CycleObject,
    MUIA_Cycle_Entries, (IPTR)ColorFormats,
    MUIA_Cycle_Active,  (IPTR)initFmtIdx,
    End,
    End,
    End,

    /* Espaceur vertical */
    Child, RectangleObject, MUIA_Weight, 50, End,

    /* --- ZONE 2 : BOUTONS --- */
    Child, GroupObject,
    MUIA_Group_Horiz, TRUE,
    MUIA_Weight, 0,
    Child, btnConnect = MUI_MakeObject(MUIO_Button, (IPTR)"_Connect"),
    Child, btnSave    = MUI_MakeObject(MUIO_Button, (IPTR)"_Save"),
    End,
    End,
    End,
    End;

    if (!app) { CloseLibrary(MUIMasterBase); return 20; }

    /* Notifications */
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(btnConnect, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, ID_CONNECT);
    DoMethod(btnSave, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, ID_SAVE);

    /* Ouverture fenêtre */
    set(win, MUIA_Window_Open, TRUE);
    set(win, MUIA_Window_ActiveObject, (IPTR)strHost);

    while (running) {
        res = DoMethod(app, MUIM_Application_NewInput, (IPTR)&signals);

        if (res == MUIV_Application_ReturnID_Quit) {
            running = FALSE;
        }
        else if (res == ID_CONNECT) {
            get(strHost, MUIA_String_Contents, &h);
            get(strPass, MUIA_String_Contents, &p);
            get(cycFmt, MUIA_Cycle_Active, &idxFmt);

            sprintf(cmd, "Run <>NIL: ApolloVNC %s %s %s", h, ColorFormats[idxFmt], p);
            Execute((STRPTR)cmd, 0, 0);
        }
        else if (res == ID_SAVE) {
            get(strHost, MUIA_String_Contents, &h);
            get(strPass, MUIA_String_Contents, &p);
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
