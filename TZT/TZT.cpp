/**
 * This file is part of Tales of Zestiria Tweak.
 *
 * Tales of Zestiria Tweak is free software : you can redistribute it
 * and / or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Tales of Zestiria Tweak is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *     See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tales of Zestiria Tweak.
 *
 *    If no license was present, see <http://www.gnu.org/licenses/>.
**/
#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"

#include "TZT.h"
#include "cfg.h"
#include "parameter.h"
#include "utility.h"
#include "nvapi.h"
#include "dxgi.h"

#include <map>
#include <set>

#include <cstdio>

#include <windowsx.h>
#include <CommCtrl.h> // Button_GetIdealSize

#pragma comment(lib, "Comctl32.lib")

using namespace tzt;

///// Match the game's setup (Arkham Knight)
////extern "C" _declspec(dllexport) DWORD NvOptimusEnablement = 0x01;

#define TZT_VERSION_STR L"0.6.2"

INT_PTR CALLBACK  Config (HWND, UINT, WPARAM, LPARAM);

bool  messagebox_active; // Workaround some particularly strange behavior
bool  first_load = true; // Some settings should only be loaded once; ignore when false

HWND  hWndApp;
HICON tzt_icon;
HICON nv_icon;

TO12_Config config;

int APIENTRY _tWinMain(_In_ HINSTANCE     hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR        lpCmdLine,
                       _In_ int           nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER (nCmdShow);

  tzt_icon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_TZT));
  nv_icon  = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_NV));

  //INT_PTR CALLBACK DriverConfigNV (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
  //return (int)DialogBox (hInstance, MAKEINTRESOURCE (IDD_DRIVER_TWEAKS), NULL, DriverConfigNV);
  return (int)DialogBox (hInstance, MAKEINTRESOURCE (IDD_TZT), NULL, Config);
}

void get_resolution (HWND hDlg, int* x, int* y, int* refresh)
{
  HWND hWndResolution = GetDlgItem (hDlg, IDC_RESOLUTION);
  HWND hWndRefresh    = GetDlgItem (hDlg, IDC_REFRESH_RATE);

  wchar_t wszRes [64];
  ComboBox_GetText (hWndResolution, wszRes, 64);

  wchar_t wszRef [16];
  ComboBox_GetText (hWndRefresh, wszRef, 16);

  swscanf_s (wszRes, L"%dx%d", x, y);
  swscanf_s (wszRef, L"%d", refresh);
}

void setup_resolution (HWND hDlg)
{
  DEVMODE dmDisplay;
  ZeroMemory (&dmDisplay, sizeof DEVMODE);
  dmDisplay.dmSize = sizeof DEVMODE;

  // X,Y pairs are mapped to sets of refresh rates
  std::map <std::pair <int, int>, std::set <int> > resolutions;

  int i = 0;
  while (EnumDisplaySettings (NULL, i, &dmDisplay)) {
    i++;

    std::pair <int, int> res;
    res.first  = dmDisplay.dmPelsWidth;
    res.second = dmDisplay.dmPelsHeight;
    resolutions [res].insert (dmDisplay.dmDisplayFrequency);
  }

  std::map <std::pair <int, int>, std::set <int> >::const_iterator res = resolutions.begin ();
  std::map <std::pair <int, int>, std::set <int> >::const_iterator end = resolutions.end ();

  HWND hWndResolution = GetDlgItem (hDlg, IDC_RESOLUTION);
  HWND hWndRefresh    = GetDlgItem (hDlg, IDC_REFRESH_RATE);

  ComboBox_ResetContent (hWndResolution);
  ComboBox_ResetContent (hWndRefresh);

  int sel = -1;
        i =  0;
  while (res != end) {
    wchar_t wszRes [64];
    swprintf (wszRes, 64, L"%dx%d", res->first.first, res->first.second);
    ComboBox_InsertString (hWndResolution, i++, wszRes);
    if (res_x->get_value () == res->first.first &&
        res_y->get_value () == res->first.second)
      sel = (i - 1);
    ++res;
  }

  // Set to highest resolution if no valid resolution is selected
  if (sel == -1)
    sel = (i - 1);

  ComboBox_SetCurSel (hWndResolution, sel);

  res = resolutions.find (std::pair <int, int> (res_x->get_value (), res_y->get_value ()));

  if (res != resolutions.end ()) {
    std::set <int>::const_iterator refresh = res->second.begin ();

      i =  0;
    sel = -1;

    while (refresh != res->second.end ()) {
      wchar_t wszRefresh [64];
      swprintf (wszRefresh, 64, L"%d", *refresh);
      ComboBox_InsertString (hWndRefresh, i++, wszRefresh);
      if (refresh_rate->get_value () == *refresh)
        sel = (i - 1);
      ++refresh;
    }

    // Set to highest refresh if no valid refresh is selected
    if (sel == -1)
      sel = (i - 1);

    ComboBox_SetCurSel (hWndRefresh, sel);
  }
}

// This resizes a UI control, avoid calling it multiple times or it will continue
//   to shrink each time!
void setup_driver_tweaks (HWND hDlg)
{
  // If there is an NV GPU installed, display a special button!
  if (NVAPI::CountPhysicalGPUs () > 0) {
    std::wstring button_label = L"    NVIDIA Driver Tweaks\r\n    (Version: ";
    button_label += NVAPI::GetDriverVersion () + L")";

    SetWindowText (GetDlgItem (hDlg, IDC_NV_DRIVER_TWEAKS), button_label.c_str ());
    ShowWindow (GetDlgItem (hDlg, IDC_NV_DRIVER_TWEAKS), true);

    // Resize the GPU Info Text Window to fit the new button...
    RECT wnd_rect;
    GetWindowRect (GetDlgItem (hDlg, IDC_GPUINFO), &wnd_rect);
    SetWindowPos (GetDlgItem (hDlg, IDC_GPUINFO), NULL, 0, 0, wnd_rect.right - wnd_rect.left, wnd_rect.bottom - wnd_rect.top - 40, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessage  (GetDlgItem (hDlg, IDC_NV_DRIVER_TWEAKS), BM_SETIMAGE, IMAGE_ICON, LPARAM (nv_icon));
  }
}

std::wstring get_level_of_detail (HWND hDlg)
{
  int sel = ComboBox_GetCurSel (GetDlgItem (hDlg, IDC_LEVEL_OF_DETAIL));

  switch (sel) {
    case 0:
      return L"near";
    case 1:
      return L"medium";
    case 2:
    default:
      return L"far";
  }
}

void setup_shadow_quality (HWND hDlg)
{
  HWND hWndShadowQuality = GetDlgItem (hDlg, IDC_SHADOW_QUALITY);

  ComboBox_ResetContent (hWndShadowQuality);
  ComboBox_InsertString (hWndShadowQuality, 0, L"Off");
  ComboBox_InsertString (hWndShadowQuality, 1, L"Low");
  ComboBox_InsertString (hWndShadowQuality, 2, L"Medium");
  ComboBox_InsertString (hWndShadowQuality, 3, L"High");

  std::wstring shadows = shadow_quality->get_value ();

  if (shadows == L"off")
    ComboBox_SetCurSel (hWndShadowQuality, 0);
  else if (shadows == L"low")
    ComboBox_SetCurSel (hWndShadowQuality, 1);
  else if (shadows == L"medium")
    ComboBox_SetCurSel (hWndShadowQuality, 2);
  else /*if (shadows == L"high")*/
    ComboBox_SetCurSel (hWndShadowQuality, 3);
}

std::wstring get_shadow_quality (HWND hDlg)
{
  int sel = ComboBox_GetCurSel (GetDlgItem (hDlg, IDC_SHADOW_QUALITY));

  if (sel == 0)
    return L"off";
  else if (sel == 1)
    return L"low";
  else if (sel == 2)
    return L"medium";
  else /*if (sel == 3)*/
    return L"high";
}


void setup_antialiasing (HWND hDlg)
{
  HWND hWndAntialiasing = GetDlgItem (hDlg, IDC_FXAA_LEVEL);

  ComboBox_ResetContent (hWndAntialiasing);
  ComboBox_InsertString (hWndAntialiasing, 0, L"Off");
  ComboBox_InsertString (hWndAntialiasing, 1, L"Low");
  ComboBox_InsertString (hWndAntialiasing, 2, L"Medium");
  ComboBox_InsertString (hWndAntialiasing, 3, L"High");

  std::wstring fxaa = antialiasing->get_value ();

  if (fxaa == L"off")
    ComboBox_SetCurSel (hWndAntialiasing, 0);
  else if (fxaa == L"fxaa_low")
    ComboBox_SetCurSel (hWndAntialiasing, 1);
  else if (fxaa == L"fxaa_medium")
    ComboBox_SetCurSel (hWndAntialiasing, 2);
  else /*if (fxaa == L"fxaa_high")*/
    ComboBox_SetCurSel (hWndAntialiasing, 3);
}

std::wstring get_antialiasing (HWND hDlg)
{
  int sel = ComboBox_GetCurSel (GetDlgItem (hDlg, IDC_FXAA_LEVEL));

  if (sel == 0)
    return L"off";
  else if (sel == 1)
    return L"fxaa_low";
  else if (sel == 2)
    return L"fxaa_medium";
  else /*if (sel == 3)*/
    return L"fxaa_high";
}


void setup_draw_distance (HWND hDlg)
{
  HWND hWndDrawDistance = GetDlgItem (hDlg, IDC_DRAW_DISTANCE);

  ComboBox_ResetContent (hWndDrawDistance);
  ComboBox_InsertString (hWndDrawDistance, 0, L"Near");
  ComboBox_InsertString (hWndDrawDistance, 1, L"Medium");
  ComboBox_InsertString (hWndDrawDistance, 2, L"Far");

  std::wstring dist = draw_distance->get_value ();

  if (dist == L"near")
    ComboBox_SetCurSel (hWndDrawDistance, 0);
  else if (dist == L"medium")
    ComboBox_SetCurSel (hWndDrawDistance, 1);
  else /*if (dist == L"far")*/
    ComboBox_SetCurSel (hWndDrawDistance, 2);
}

std::wstring get_draw_distance (HWND hDlg)
{
  int sel = ComboBox_GetCurSel (GetDlgItem (hDlg, IDC_DRAW_DISTANCE));

  if (sel == 0)
    return L"near";
  else if (sel == 1)
    return L"medium";
  else /*if (sel == 2)*/
    return L"far";
}


void setup_level_of_detail (HWND hDlg)
{
  HWND hWnd_LOD = GetDlgItem (hDlg, IDC_LEVEL_OF_DETAIL);

  ComboBox_ResetContent (hWnd_LOD);
  ComboBox_InsertString (hWnd_LOD, 0, L"Near");
  ComboBox_InsertString (hWnd_LOD, 1, L"Medium");
  ComboBox_InsertString (hWnd_LOD, 2, L"Far");

  std::wstring lod = level_of_detail->get_value ();

  if (lod == L"near")
    ComboBox_SetCurSel (hWnd_LOD, 0);
  else if (lod == L"medium")
    ComboBox_SetCurSel (hWnd_LOD, 1);
  else //if (lod == L"far")
    ComboBox_SetCurSel (hWnd_LOD, 2);
}

void setup_tex_filter (HWND hDlg)
{
  HWND hWndAnisotropy = GetDlgItem (hDlg, IDC_ANISOTROPY);

  ComboBox_ResetContent (hWndAnisotropy);
  ComboBox_InsertString (hWndAnisotropy, 0, L"Trilinear");
  ComboBox_InsertString (hWndAnisotropy, 1, L"2x Anisotropic");
  ComboBox_InsertString (hWndAnisotropy, 2, L"4x Anisotropic");
  ComboBox_InsertString (hWndAnisotropy, 3, L"8x Anisotropic");
  ComboBox_InsertString (hWndAnisotropy, 4, L"16x Anisotropic");

  switch (anisotropy->get_value ()) {
    case 1:
      // 1 sample = Bilinear + Linear/Point filter on Mip
      ComboBox_SetCurSel (hWndAnisotropy, 0);
      break;
    case 2:
      // 2X AF
      ComboBox_SetCurSel (hWndAnisotropy, 1);
      break;

      /// Believe it or not, anisotropy doesn't have to be a power-of-two
      ///   or even an integer... but the engine might not accept
      ///     those sorts of values, so let's not confuse it ;)

    default: // Always fallback to 4x AF if bad values are passed.
    case 4:  // 4x AF
      ComboBox_SetCurSel (hWndAnisotropy, 2);
      break;
    case 8: // 8x AF
      ComboBox_SetCurSel (hWndAnisotropy, 3);
      break;
    case 16: // 16x AF
      ComboBox_SetCurSel (hWndAnisotropy, 4);
      break;
  }
}

int get_tex_filter (HWND hDlg)
{
  int sel = ComboBox_GetCurSel (GetDlgItem (hDlg, IDC_ANISOTROPY));

  if (sel == 0)
    return 1;
  else if (sel == 1)
    return 2;
  else if (sel == 2)
    return 4;
  else if (sel == 3)
    return 8;
  else
    return 16;
}

void setup_tex_res (HWND hDlg)
{
  HWND hWndTexRes = GetDlgItem (hDlg, IDC_TEXTURE_RES);

  ComboBox_ResetContent (hWndTexRes);
  ComboBox_InsertString (hWndTexRes, 0, L"CANNOT ADJUST ME");//Low (128x128 - 512x512) - 2 GiB VRAM");
  ComboBox_InsertString (hWndTexRes, 1, L"Normal (128x128 - 1024x1024) - 4 GiB VRAM");
  ComboBox_InsertString (hWndTexRes, 2, L"High (128x128 - 2048x2048) - 6 GiB VRAM");

  if (texture_res->get_value () == 2)      // High
    ComboBox_SetCurSel (hWndTexRes, 2);
  else if (texture_res->get_value () == 1) // Med
    ComboBox_SetCurSel (hWndTexRes, 1);
  else
    ComboBox_SetCurSel (hWndTexRes, 0);    // Anything Else = Low
}

int get_tex_res (HWND hDlg)
{
  return ComboBox_GetCurSel (GetDlgItem (hDlg, IDC_TEXTURE_RES));
}

bool TZT_EpsilonTest (float in, float test)
{
  const float eps = 0.0001f;

  if (in + eps > test && in - eps < test)
    return true;

  return false;
}

INT_PTR CALLBACK DriverConfigNV (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);



// What a nasty kludge this is, fix this ASAP!!!!
int mode = 0;

void handle_window_radios (HWND hDlg, WORD ID)
{
  switch (ID) {
    case IDC_BORDER_WINDOW:
      mode = 0;
      break;
    case IDC_BORDERLESS_WINDOW:
      if (mode == 1)
        mode = 3;
      else
        mode = 1;
      break;
    case IDC_FULLSCREEN:
      mode = 2;
      break;
  }

  if (mode < 3) {
    LONG style = GetWindowLong (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), GWL_STYLE);
         style = (style & ~BS_AUTO3STATE) | BS_AUTORADIOBUTTON;
    SetWindowLong (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), GWL_STYLE, style);
  }

  if (mode == 0) {
    Button_SetCheck (GetDlgItem (hDlg, IDC_BORDER_WINDOW),     1);
    Button_SetCheck (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), 0);
    Button_SetCheck (GetDlgItem (hDlg, IDC_FULLSCREEN),        0);
  }
  else if (mode == 2) {
    Button_SetCheck (GetDlgItem (hDlg, IDC_BORDER_WINDOW),     0);
    Button_SetCheck (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), 0);
    Button_SetCheck (GetDlgItem (hDlg, IDC_FULLSCREEN),        1);
  }
  else {
    Button_SetCheck (GetDlgItem (hDlg, IDC_BORDER_WINDOW), 0);
    Button_SetCheck (GetDlgItem (hDlg, IDC_FULLSCREEN),    0);

    // Regular borderless
    if (mode == 1) {
      LONG style = GetWindowLong (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), GWL_STYLE);
           style = (style & ~BS_AUTO3STATE) | BS_AUTORADIOBUTTON;
      SetWindowLong   (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), GWL_STYLE, style);
      Button_SetCheck (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), 1);
    }
    else {
      LONG style = GetWindowLong (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), GWL_STYLE);
           style = (style & ~BS_AUTORADIOBUTTON) | BS_AUTO3STATE;
      SetWindowLong   (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), GWL_STYLE, style);
      Button_SetCheck (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW), 2);
    }
  }

  // Only enable this button in fullscreen mode...
  Button_Enable (GetDlgItem (hDlg, IDC_VSYNC), (mode == 2));

  if (mode != 2)
    Button_SetCheck (GetDlgItem (hDlg, IDC_VSYNC), 0);
  else
    Button_SetCheck (GetDlgItem (hDlg, IDC_VSYNC), use_vsync->get_value ());
}

void setup_debug_utils (HWND hDlg, bool debug)
{
  ShowWindow (GetDlgItem (hDlg, IDC_GPUINFO),          (! debug));
  ShowWindow (GetDlgItem (hDlg, IDC_GPU_GROUP),        (! debug));
  if (NVAPI::CountPhysicalGPUs () > 0) {
    // Show/hide the NVIDIA driver tweaks button if applicable

    ShowWindow (GetDlgItem (hDlg, IDC_NV_DRIVER_TWEAKS), (!debug));
  }
  ShowWindow (GetDlgItem (hDlg, IDC_DEBUG_GROUP),         debug);
  ShowWindow (GetDlgItem (hDlg, IDC_NUKE_CONFIG),         debug);
  ShowWindow (GetDlgItem (hDlg, IDC_BACKUP_CONFIG),       debug);
  ShowWindow (GetDlgItem (hDlg, IDC_RESTORE_CONFIG),      debug);

  if (debug) {
    if (TZT_HasBackupConfigFiles ()) {
      std::wstring backup_time = TZT_GetBackupFileTime ();
      SetWindowText (GetDlgItem (hDlg, IDC_RESTORE_CONFIG), std::wstring (L" Restore Config Files\n " +
                                                                            backup_time).c_str ());
      EnableWindow (GetDlgItem (hDlg, IDC_RESTORE_CONFIG), TRUE);
      //EnableWindow (GetDlgItem (hDlg, IDC_BACKUP_CONFIG), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_BACKUP_CONFIG),  TRUE);
      std::wstring config_time = TZT_GetConfigFileTime ();
      SetWindowText (GetDlgItem (hDlg, IDC_BACKUP_CONFIG), std::wstring (L" Backup Config Files\n " +
                                                                         config_time).c_str ());
    }
    else {
      std::wstring config_time = TZT_GetConfigFileTime ();
      SetWindowText (GetDlgItem (hDlg, IDC_BACKUP_CONFIG), std::wstring (L" Backup Config Files\n " +
                                                                         config_time).c_str ());
      EnableWindow (GetDlgItem (hDlg, IDC_BACKUP_CONFIG),  TRUE);
      EnableWindow (GetDlgItem (hDlg, IDC_RESTORE_CONFIG), FALSE);
      SetWindowText (GetDlgItem (hDlg, IDC_RESTORE_CONFIG), L" Restore Config Files");
    }
  }
}


#include <mmeapi.h>
#include <dsound.h>
#pragma comment (lib, "dsound.lib")

wchar_t wszConfigStatus [16384];

const wchar_t*
dsound_channel_config_name (DWORD dwLayout)
{
  switch (dwLayout)
  {
    case DSSPEAKER_DIRECTOUT:
      return L"Direct Out";
    case DSSPEAKER_HEADPHONE:
      return L"Headphone";
    case DSSPEAKER_MONO:
      return L"Monophonic";
    case DSSPEAKER_QUAD:
      return L"Quadraphonic";
    case DSSPEAKER_STEREO:
      return L"Stereo";
    case DSSPEAKER_SURROUND:
      return L"Surround";
    case DSSPEAKER_5POINT1:
      return L"5.1 (Obsolete Setting - AVOIDME)";
    case DSSPEAKER_7POINT1:
      return L"7.1 (Obsolete Setting - AVOIDME)";
    case DSSPEAKER_7POINT1_SURROUND:
      return L"7.1 Surround";
    case DSSPEAKER_5POINT1_SURROUND:
      return L"5.1 Surround";
    default:
      return L"UNKNOWN?!";
  }
}

std::wstring
dsound_speaker_geometry_name (DWORD dwGeometry)
{
  switch (dwGeometry)
  {
    case DSSPEAKER_GEOMETRY_MIN:
      return L"Minimum (5�)";
    case DSSPEAKER_GEOMETRY_NARROW:
      return L"Narrow (10�)";
    case DSSPEAKER_GEOMETRY_WIDE:
      return L"Wide (20�)";
    case DSSPEAKER_GEOMETRY_MAX:
      return L"Maximum (180�)";
    default:
    {
      wchar_t wszCustom [64];
      swprintf (wszCustom, L"Custom (%lu�)", dwGeometry);
      return wszCustom;
    }
  }
}

#include <initguid.h>
#pragma comment (lib, "ksproxy.lib")
#include <ks.h>
//#include <ksproxy.h>
#include <ksmedia.h>

#include <Mmdeviceapi.h>
#include <audioendpoints.h>
#include <audioclient.h>

void
setup_config_status (HWND hDlg)
{
  wchar_t* pwszStat = wszConfigStatus;

  pwszStat += swprintf (pwszStat, L"Creating Default DirectSound Device... ");

  LPDIRECTSOUND8 pDS;
  if (SUCCEEDED (DirectSoundCreate8 (NULL, &pDS, NULL))) {
    pDS->Initialize (NULL);
    pwszStat += swprintf (pwszStat, L"Success!\r\n");

    pwszStat += swprintf (pwszStat, L" >> Retrieving Channel Configuration...\t");
    DWORD dwSpeakerConfig;
    pDS->GetSpeakerConfig (&dwSpeakerConfig);
    pwszStat += swprintf (pwszStat, L"%d\t(%s)\r\n",
                                DSSPEAKER_CONFIG (dwSpeakerConfig),
                      dsound_channel_config_name (
                                DSSPEAKER_CONFIG (dwSpeakerConfig)));

#if 0
    pwszStat += swprintf (pwszStat, L" >> Retrieving Speaker Geometry...\t\t");
    pwszStat += swprintf (pwszStat, L"%d\t(%s)\r\n",
        DSSPEAKER_GEOMETRY (dwSpeakerConfig),
      dsound_speaker_geometry_name (
        DSSPEAKER_GEOMETRY (dwSpeakerConfig)).c_str ());
#endif

    DSCAPS sound_caps;
    if (SUCCEEDED (pDS->GetCaps (&sound_caps))) {
      pwszStat += swprintf (pwszStat, L"---\r\nMinimum Sample Rate:\t%lu"
        L"\r\nMaximum Sample Rate: %lu\r\n",
        sound_caps.dwMinSecondarySampleRate,
        sound_caps.dwMaxSecondarySampleRate );
    }

    IMMDeviceEnumerator* pEnumerator;

    CoInitialize (NULL);

    const CLSID CLSID_MMDeviceEnumerator = __uuidof (MMDeviceEnumerator);
    const IID   IID_IMMDeviceEnumerator  = __uuidof (IMMDeviceEnumerator);

    HRESULT hr = CoCreateInstance (
                   CLSID_MMDeviceEnumerator, NULL,
                   CLSCTX_ALL, IID_IMMDeviceEnumerator,
                   (void **)&pEnumerator
                 );

    if (SUCCEEDED (hr)) {
      IMMDevice* pDev;

      if (SUCCEEDED (
             pEnumerator->GetDefaultAudioEndpoint (eRender, eConsole, &pDev)
           )
         )
      {
        IAudioClient* pAudioClient;
        if (SUCCEEDED (pDev->Activate (
                         __uuidof (IAudioClient), CLSCTX_ALL,
                         NULL, (void **)&pAudioClient )
                      )
           ) {
          WAVEFORMATEX*   pMixFormat;
          IPropertyStore* pStore;
          PROPVARIANT     property;

          if (SUCCEEDED (pDev->OpenPropertyStore (STGM_READ, &pStore)))
          {
            if (SUCCEEDED (pStore->GetValue (PKEY_AudioEngine_DeviceFormat, &property)))
            {
              pMixFormat = (PWAVEFORMATEX)property.blob.pBlobData;

              //pAudioClient->GetMixFormat ((WAVEFORMATEX **)&pMixFormat);

              pwszStat += swprintf (pwszStat, L"---\r\n * Default Playback Endpoint:\r\n"
                                              L"\tChannel Count.:\t%lu\r\n"
                                              L"\tSample Rate...:\t%4.1f kHz\r\n"
                                              L"\tData Type.....:\t%hu-Bits\r\n",
                                                pMixFormat->nChannels,
                                                (float)pMixFormat->nSamplesPerSec / 1000.0f,
                                                pMixFormat->wBitsPerSample);

          //pMixFormat->wBitsPerSample = ((PWAVEFORMATEXTENSIBLE)pMixFormat)->Samples.wValidBitsPerSample;

          //pMixFormat->nChannels = 4;
          //pStore->SetValue        (PKEY_AudioEngine_DeviceFormat, property);

          if (pMixFormat->nChannels > 6) {
#if 0
            MessageBox ( NULL,
                           L"Too many audio channels!"
                           L"\tPlease select a speaker configuration or device "
                            L"with 5.1 channels or fewer",
                             L"Bad Audio Configuration",
                               MB_OK | MB_ICONSTOP );

            char snd_cfg [MAX_PATH];
            GetSystemDirectoryA (snd_cfg, MAX_PATH);
            lstrcatA (snd_cfg, "\\control.exe mmsys.cpl");

            WinExec (snd_cfg, SW_SHOW);
#endif
          }
          }
          pStore->Release ();
          }
        }
        pDev->Release ();
      }
      pEnumerator->Release ();
    }

    pwszStat += swprintf (pwszStat, L"Releasing Default DirectSound Device... ");
    pDS->Release ();
    pwszStat += swprintf (pwszStat, L"done!\r\n");
  } else {
    pwszStat += swprintf (pwszStat, L"Failed!\r\n");
  }

  Edit_NoSetFocus (GetDlgItem (hDlg, IDC_TZF_LOG));
  Edit_SetText (GetDlgItem (hDlg, IDC_TZF_LOG), wszConfigStatus);
  Edit_NoSetFocus (GetDlgItem (hDlg, IDC_TZF_LOG));
}


using namespace tzt;
using namespace tzt::UI;

INT_PTR
CALLBACK
Config (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_INITDIALOG:
    {
      SendMessage (hDlg, WM_SETICON, ICON_BIG,   (LPARAM)tzt_icon);
      SendMessage (hDlg, WM_SETICON, ICON_SMALL, (LPARAM)tzt_icon);

      // Wow this code is ugly, it all needs to be wrapped...
      HINSTANCE hShell32 = LoadLibrary (L"shell32.dll");
      HICON     hIcon    = LoadIcon    (hShell32, MAKEINTRESOURCE (16761));

      SIZE size;
      SendMessage (GetDlgItem (hDlg, IDOK), BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
      Button_GetIdealSize (GetDlgItem (hDlg, IDOK), &size);
      SetWindowPos (GetDlgItem (hDlg, IDOK), NULL, 0, 0, size.cx + 6, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

      hIcon = LoadIcon (hShell32, MAKEINTRESOURCE (200));

      SendMessage (GetDlgItem (hDlg, IDCANCEL), BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);
      Button_GetIdealSize (GetDlgItem (hDlg, IDCANCEL), &size);
      SetWindowPos (GetDlgItem (hDlg, IDCANCEL), NULL, 0, 0, size.cx + 6, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

      hIcon = LoadIcon (hShell32, MAKEINTRESOURCE (145));
      SendMessage (GetDlgItem (hDlg, IDC_NUKE_CONFIG), BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

      hIcon = LoadIcon (hShell32, MAKEINTRESOURCE (16771));
      SendMessage (GetDlgItem (hDlg, IDC_BACKUP_CONFIG), BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

      hIcon = LoadIcon (hShell32, MAKEINTRESOURCE (16741));
      SendMessage (GetDlgItem (hDlg, IDC_RESTORE_CONFIG), BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

      FreeLibrary (hShell32);

      SetWindowText (hDlg, L"Tales of Zestiria (TO12) Tweak v " TZT_VERSION_STR);

      hWndApp = hDlg;

      NVAPI::InitializeLibrary ();

      // Make sure if the config file does not alread exist, we generate one before trying to parse it...
      TZT_InitializeConfig ();

      std::wstring config_path (TZT_GetLocalAppDataDir () + L"\\BANDAI NAMCO Games\\Tales of Zestiria\\");
      config.load (config_path); 

      // Setup the config variables on first load only
      if (first_load) {
        refresh_rate =
          static_cast <ParameterInt *> (
            g_ParameterFactory.create_parameter <int> (L"Refresh Rate")
          );

        res_x =
          static_cast <ParameterInt *> (
            g_ParameterFactory.create_parameter <int> (L"X Resolution")
          );

        res_y =
          static_cast <ParameterInt *> (
            g_ParameterFactory.create_parameter <int> (L"Y Resolution")
          );

        //max_fps =
          //static_cast <ParameterInt *> (
            //g_ParameterFactory.create_parameter <int> (L"Maximum Framerate")
          //);

        use_vsync =
          static_cast <ParameterBool *> (
            g_ParameterFactory.create_parameter <bool> (L"Use VSYNC")
          );

        // In some APIs this is a float, but let's just keep things simple (int).
        anisotropy =
          static_cast <ParameterInt *> (
            g_ParameterFactory.create_parameter <int> (L"Maximum Anisotropic Filter")
          );

        texture_res =
          static_cast <ParameterInt *> (
            g_ParameterFactory.create_parameter <int> (L"Texture Resolution Level")
          );

        level_of_detail =
          static_cast <ParameterStringW *> (
            g_ParameterFactory.create_parameter <std::wstring> (L"Level of Detail")
          );

        shadow_quality =
          static_cast <ParameterStringW *> (
            g_ParameterFactory.create_parameter <std::wstring> (L"Shadow Quality")
          );

        antialiasing =
          static_cast <ParameterStringW *> (
            g_ParameterFactory.create_parameter <std::wstring> (L"Antialiasing")
          );

        draw_distance =
          static_cast <ParameterStringW *> (
            g_ParameterFactory.create_parameter <std::wstring> (L"Draw Distance")
          );

        //framerate_limiting =
          //static_cast <ParameterInt *> (
            //g_ParameterFactory.create_parameter <int> (L"Frame Rate Limiting Setting (not what it sounds like)")
          //);
      }

      refresh_rate->register_to_cfg (config.get_file (), L"refreshRateHz");
      refresh_rate->load ();

      res_x->register_to_cfg (config.get_file (), L"resolution_X");
      res_x->load ();

      res_y->register_to_cfg (config.get_file (), L"resolution_Y");
      res_y->load ();

      //max_fps->register_to_ini (settings.get_file (), L"SystemSettings", L"MaxFPS");
      //max_fps->bind_to_control (new EditBox (GetDlgItem (hDlg, IDC_MAXFPS)));
      //max_fps->load ();


      use_vsync->register_to_cfg (config.get_file (), L"vsync");
      use_vsync->bind_to_control (new CheckBox (GetDlgItem (hDlg, IDC_VSYNC)));
      use_vsync->load ();


      anisotropy->register_to_cfg (config.get_file (), L"anisotropy");
      anisotropy->load ();

      //texture_res->register_to_ini (settings.get_file (), L"SystemSettings", L"TextureResolution");
      //texture_res->load ();


      level_of_detail->register_to_cfg (config.get_file (), L"lod_distance");
      level_of_detail->load ();

      shadow_quality->register_to_cfg (config.get_file (), L"shadows");
      shadow_quality->load ();

      antialiasing->register_to_cfg (config.get_file (), L"antialiasing");
      antialiasing->load ();

      draw_distance->register_to_cfg (config.get_file (), L"draw_distance");
      draw_distance->load ();

      //framerate_limiting->register_to_ini (engine.get_file (), L"Engine.Engine", L"FrameRateLimitingSetting");
      //framerate_limiting->load ();

#if 0
      //
      // Now, for the non-game settings -- [BMT.User]
      //
      if (first_load) { // Keep this preference across configuration restorations
        decline_backup =
          static_cast <ParameterBool *> (
            g_ParameterFactory.create_parameter <bool> (L"NoBackup")
          );

        decline_backup->register_to_ini (settings.get_file (), L"BMT.User", L"NoBackup");
        decline_backup->load ();
      }
      else {
        // This isn't enough, we need to actually save BmSystemSettings.ini to make sure
        //   config files retain this preference -- @FIXME: Persistent backup preference.
        decline_backup->store ();
      }
#endif

      setup_resolution       (hDlg);
      setup_tex_filter       (hDlg);
      setup_tex_res          (hDlg);
      setup_level_of_detail  (hDlg);
      setup_shadow_quality   (hDlg);
      setup_antialiasing     (hDlg);
      setup_draw_distance    (hDlg);

      setup_driver_tweaks    (hDlg);
      setup_config_status    (hDlg);

      //setup_framerate_limiting (hDlg);

      std::wstring gpu_str = DXGI::GetGPUInfo ();

      HFONT font = CreateFont (11, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, MONO_FONT, L"Consolas");

      SetWindowFont (GetDlgItem (hDlg, IDC_GPUINFO), font, true);
      Edit_SetText (GetDlgItem (hDlg, IDC_GPUINFO), gpu_str.c_str ());

      SetActiveWindow (GetDlgItem (hDlg, IDOK));
      SetFocus        (GetDlgItem (hDlg, IDOK));

      EnableWindow    (GetDlgItem (hDlg, IDC_TEXTURE_RES), FALSE);

      int window_mode =
        _wtoi (config.lookup_value (L"GLOBAL", L"fullscreen").c_str ());

      // @TODO: Radio button wrapper, doing it this way is just plain ridiculous!

      // Disable the regular old windowed mode button, because ... the game des not support that.
      EnableWindow (GetDlgItem (hDlg, IDC_BORDER_WINDOW), FALSE);

      if (window_mode == 0) {
        //mode = 0;
        //handle_window_radios (hDlg, IDC_BORDER_WINDOW);
        mode = 1;
        handle_window_radios (hDlg, IDC_BORDERLESS_WINDOW);
      }
      /*
      else if (window_mode == 1) {
        // We have two borderless modes, one standard and this special one (change desktop res at launch)
        if (config.lookup_value (L"GLOBAL", L"fullscreen") == L"3") {
          mode = 1;
          handle_window_radios (hDlg, IDC_BORDERLESS_WINDOW);
        }
        else {
          mode = 3;
          handle_window_radios (hDlg, IDC_BORDERLESS_WINDOW);
        }
      }
      */
      else {
        mode = 2;
        handle_window_radios (hDlg, IDC_FULLSCREEN);
      }

      first_load = false;

      return (INT_PTR)TRUE;
    }

    case WM_LBUTTONDBLCLK:
    {
      bool debug = IsWindowVisible (GetDlgItem (hDlg, IDC_DEBUG_GROUP));
      debug = !debug;

      setup_debug_utils (hDlg, debug);
    } break;

    case WM_COMMAND:
    {
      // What insanity is this?! The message pump shouldn't be working while a message box is up!
      if (messagebox_active) // Ignore button clicks while a Message Box is active
        return 0;

      if (LOWORD (wParam) == IDC_BORDERLESS_WINDOW || 
          LOWORD (wParam) == IDC_BORDER_WINDOW     ||
          LOWORD (wParam) == IDC_FULLSCREEN) {
        handle_window_radios (hDlg, LOWORD (wParam));
      }
      if (LOWORD (wParam) == IDC_MATCH_DESKTOP) {
        DEVMODE dmNow;

        memset (&dmNow, 0, sizeof (DEVMODE));
            dmNow.dmSize = sizeof (DEVMODE);

        EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &dmNow);

        res_x->set_value        (dmNow.dmPelsWidth);
        res_y->set_value        (dmNow.dmPelsHeight);
        refresh_rate->set_value (dmNow.dmDisplayFrequency);

        setup_resolution (hDlg);
      }

      if (LOWORD (wParam) == IDC_NV_DRIVER_TWEAKS) {
        NVAPI::tzt_fullscreen = (mode == 2); // This is a hack based on another hack (see handle_window_radios), yuck.
        return (int)DialogBox (GetWindowInstance (hDlg), MAKEINTRESOURCE (IDD_DRIVER_TWEAKS), hDlg, DriverConfigNV);
      }

      if (LOWORD (wParam) == IDC_RESOLUTION) {
        if (HIWORD (wParam) == CBN_SELCHANGE) {
          int x, y, refresh;
          get_resolution (hDlg, &x, &y, &refresh);

          res_x->set_value (x);
          res_y->set_value (y);

          // Set the refresh rate whenever the resolution changes...
          setup_resolution (hDlg);
        }
      }

      // Update refresh rate immediately
      if (LOWORD (wParam) == IDC_REFRESH_RATE) {
        if (HIWORD (wParam) == CBN_SELCHANGE) {
          int x, y, refresh;
          get_resolution (hDlg, &x, &y, &refresh);
          refresh_rate->set_value (refresh);
        }
      }

      if (LOWORD (wParam) == IDC_NUKE_CONFIG)
      {
        if (MessageBox (NULL, L"WARNING: This will delete all game settings and restore you to default settings the next time you run the game.\n\n"
                              L"\tDo you really wish to continue?",
                              L"Please Confirm An Irreversible Operation",
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDOK) {
          TZT_DeleteAllConfigFiles ();
          EndDialog (hDlg, LOWORD (wParam));
          return (INT_PTR)TRUE;
        }
      }

      if (LOWORD (wParam) == IDC_RESTORE_CONFIG)
      {
        int status =
          TZT_MessageBox (L"This will discard any changes you've made and restore configuration to an earlier state.\n\n"
                          L"\tDo you wish to continue?",
                          L"Restore Configuration From Backup?",
            MB_YESNO | MB_ICONQUESTION);

        if (status == IDYES) {

          HINSTANCE hInst = GetWindowInstance (hDlg);

          TZT_RestoreConfigFiles ();
          EndDialog (hDlg, LOWORD (wParam));

          // We're effectively going to recurse here, there's a slim
          //  possibility of a stack overflow if the user does this enough.
          return (int)DialogBox (hInst, MAKEINTRESOURCE (IDD_TZT), NULL, Config);
        }
      }

      if (LOWORD (wParam) == IDC_BACKUP_CONFIG)
      {
        bool allow_backup = true;

        // Confirm before overwriting an existing backup.
        if (TZT_HasBackupConfigFiles ()) {
          int status =
            TZT_MessageBox (L"A previous backup already exists.\n\n"
                            L"\tWould you like to replace it?",
                            L"Replace Existing Configuration Backup?",
              MB_YESNO | MB_ICONQUESTION);

          if (status != IDYES)
            allow_backup = false;
        }

        // Honor the user's selection
        if (allow_backup)
          TZT_CreateBackupConfig ();

        // Basically just refresh the file timestamps
        setup_debug_utils (hDlg, true);
      }

      if (LOWORD (wParam) == IDCANCEL)
      {
        NVAPI::UnloadLibrary ();
        EndDialog (hDlg, LOWORD (wParam));
        return (INT_PTR)TRUE;
      }

      else if (LOWORD (wParam) == IDOK) {
        int x, y, refresh;
        get_resolution (hDlg, &x, &y, &refresh);

        res_x->set_value    (x);
        res_x->store        ();

        res_y->set_value    (y);
        res_y->store        ();

        refresh_rate->set_value (refresh);
        refresh_rate->store     ();

        //max_fps->store      ();

        use_vsync->store    ();

        anisotropy->set_value (get_tex_filter (hDlg));
        anisotropy->store     ();

        ////

        level_of_detail->set_value (get_level_of_detail (hDlg));
        level_of_detail->store     ();

        shadow_quality->set_value (get_shadow_quality (hDlg));
        shadow_quality->store     ();

        antialiasing->set_value   (get_antialiasing (hDlg));
        antialiasing->store       ();

        draw_distance->set_value  (get_draw_distance (hDlg));
        draw_distance->store      ();

        texture_res->set_value (get_tex_res (hDlg));
        texture_res->store     ();

        //framerate_limiting->set_value (get_framerate_limiting (hDlg));
        //framerate_limiting->set_value (7);
        //framerate_limiting->store     ();

        int window_mode = 0;

        if (Button_GetCheck (GetDlgItem (hDlg, IDC_BORDER_WINDOW))) {
          window_mode = 0;
          config.set_value (L"GLOBAL", L"fullscreen", L"0");
        }

        if (Button_GetCheck (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW)) == 1) {
          window_mode = 1;
          ///display_mode_val->value (L"1");
          config.set_value (L"GLOBAL", L"fullscreen", L"0");
        }

        // We created a custom 4th mode, in which BMT changes the desktop resolution...
        if (Button_GetCheck (GetDlgItem (hDlg, IDC_BORDERLESS_WINDOW)) == 2) {
          window_mode = 1;
          ///display_mode_val->value (L"1");
          config.set_value (L"GLOBAL", L"fullscreen", L"0");
          //settings.set_value (L"SystemSettings", L"WindowDisplayMode", L"3");
        }

        if (Button_GetCheck (GetDlgItem (hDlg, IDC_FULLSCREEN))) {
          window_mode = 2;
          ///display_mode_val->value (L"2");
          config.set_value (L"GLOBAL", L"fullscreen", L"1");
        }

        bool cancel = false;

        // System to make an automatic backup if INI files at save-time
        if (! TZT_HasBackupConfigFiles ()) {
          // So, no backups exist - maybe the user doesn't want backups?
          //if (! decline_backup->get_value ()) {
            //int status =
              //TZT_MessageBox (L"No backup configuration files were detected, would you like to backup your configuration now?\n\n\tIf you press [No], you will never be prompted again.",
                              //L"Create Backup Configuration Files?",
                              //MB_YESNOCANCEL | MB_ICONQUESTION);

            //if (status == IDCANCEL)
              //return (INT_PTR)TRUE; 

            //if (status == IDYES) {
              TZT_CreateBackupConfig    ();
              //decline_backup->set_value (false);
            //}
            //else {
              //decline_backup->set_value (true);
            //}

            //decline_backup->store ();
          //}
        }

        std::wstring config_path (TZT_GetLocalAppDataDir () + L"\\BANDAI NAMCO Games\\Tales of Zestiria\\");
        config.save (config_path);

        NVAPI::UnloadLibrary ();

        EndDialog (hDlg, TRUE);
        return (INT_PTR)TRUE;
      }
    } break;

    case WM_DESTROY:
    {
      NVAPI::UnloadLibrary ();
      //int ret = MessageBox (NULL, L"Quit Without Saving?", L"Settings Have Not Been Saved", MB_YESNOCANCEL);

      //if (ret == IDOK)
        PostQuitMessage (0);
      //else

    } break;
  }

  return (INT_PTR)FALSE;
}