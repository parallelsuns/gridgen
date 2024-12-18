//=======================================================================
/** @file main.cpp
 *  @author Benedict Lee
 *  @copyright Copyright (C) 2024 Benedict Lee
 *
 * This file is part of the 'GridGen' application
 *
 * MIT License
 *
 * Copyright (C) 2024 Benedict Lee
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
 //=======================================================================

#include "framework.h"
#include "AudioFile.h"
#include "GridGen.h"
#include <commdlg.h>
#include <vector>
#include <windowsx.h>
#include <string>
#include "libsamplerate/samplerate.h"
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
BOOL                InitInstance(HINSTANCE, int);
INT_PTR CALLBACK    DialogProc(HWND, UINT, WPARAM, LPARAM);

const int bitdepths[] = { 16,24 };
const int samplerates[] = { 11025, 12000, 22050, 24000, 44100, 48000, 88200, 96000 };
const wchar_t* channels[] = { L"Stereo", L"Mono (Left)", L"Mono (Right)", L"Mono (Mix)" };

std::vector<std::string> input_filenames;
unsigned int minGridSz = 0;
unsigned int channelMode = 0;
int outputBitDepth = 16;
int outputSamplerate = 44100;
float maxLength = 0;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDD_MAINDIALOG));
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateDialogParam(hInst,
     MAKEINTRESOURCE(IDD_MAINDIALOG),
     nullptr, DialogProc, (LPARAM)nullptr);
   SetWindowPos(hWnd, nullptr, 640, 480, 0,0,SWP_NOSIZE);
   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void ConvertToTargetSamplerate(AudioFile<float>& sample)
{
    if (sample.getSampleRate() == outputSamplerate || sample.getNumSamplesPerChannel() <= 0) return;
    AudioFile<float> new_sample;
    new_sample.setBitDepth(outputBitDepth);
    new_sample.setSampleRate(outputSamplerate);
    new_sample.setNumChannels(sample.getNumChannels());
    new_sample.setNumSamplesPerChannel(int(sample.getNumSamplesPerChannel() * ((double)outputSamplerate / sample.getSampleRate())));
    for (int ch = 0; ch < sample.getNumChannels(); ++ch)
    {
        SRC_DATA data;
        data.data_in = sample.samples[ch].data();
        data.input_frames = (long)sample.samples[ch].size();
        data.data_out = new_sample.samples[ch].data();
        data.output_frames = (long)new_sample.samples[ch].size();
        data.src_ratio = (double)outputSamplerate / (double)sample.getSampleRate();
        src_simple(&data, SRC_SINC_BEST_QUALITY, 1);
    }
    sample = new_sample;
}


void ExportSampleGrid(HWND hwnd, const std::string& output_path)
{
  if (input_filenames.size() == 0) return;
  std::vector<AudioFile<float>> input_files;
  input_files.reserve(input_filenames.size());
  float longest_len = 0;

  for (size_t i = 0; i < input_filenames.size(); ++i)
  {
    input_files.push_back(AudioFile<float>(input_filenames[i]));
    if (input_files.back().getSampleRate() != outputSamplerate)
        ConvertToTargetSamplerate(input_files[input_files.size() - 1]);
    input_files.back().setBitDepth(outputBitDepth);

    //trim sample if theres ending silence
    int last_index = input_files.back().getNumSamplesPerChannel() - 1;
    while (last_index >= 0)
    {   
        bool found_end = false;
        for (int n = 0; n < input_files.back().getNumChannels(); ++n)
        {
            if (input_files.back().samples[n][last_index] > FLT_EPSILON*2)
            {
                found_end = true;
                break;
            }
        }
        if (found_end) break;
        --last_index;
    }
    input_files.back().setNumSamplesPerChannel(last_index + 1);

    if (maxLength > 0 && maxLength < input_files.back().getLengthInSeconds())
        input_files.back().setNumSamplesPerChannel(int(input_files.back().getSampleRate() * maxLength));
    
    //find longest sample
    if (input_files.back().getLengthInSeconds() > longest_len) longest_len = input_files.back().getLengthInSeconds();
  }

  //Change channel count accordingly
  if (channelMode == 0) //stereo
  {
      for (size_t i = 0; i < input_files.size(); ++i)
      {
          if (input_files[i].getNumChannels() > 2) input_files[i].setNumChannels(2);
          else if (input_files[i].getNumChannels() == 1)
          {
              input_files[i].setNumChannels(2);
              std::copy(input_files[i].samples[0].begin(), input_files[i].samples[0].end(), input_files[i].samples[1].begin()); //copy left channel to right
          }
          else input_files[i].setNumChannels(2);
      }
  }
  else if (channelMode == 1) //mono left
  {
      for (size_t i = 0; i < input_files.size(); ++i)
          input_files[i].setNumChannels(1);
  }
  else if (channelMode == 2) //mono right
  {
      for (size_t i = 0; i < input_files.size(); ++i)
      {
          if (input_files[i].getNumChannels() == 0) input_files[i].setNumChannels(1);
          else if (input_files[i].getNumChannels() >= 2)
          {
              std::copy(input_files[i].samples[1].begin(), input_files[i].samples[1].end(), input_files[i].samples[0].begin()); //copy right channel to left
              input_files[i].setNumChannels(1);
          }
      }
  }
  else if (channelMode == 3) //mono mix
  {
      for (size_t i = 0; i < input_files.size(); ++i)
      {
          if (input_files[i].getNumChannels() != 1)
          {
              for (int m = 0; m < input_files[i].getNumSamplesPerChannel(); ++m)
              {
                  float sum = 0;
                  for (int n = 0; n < input_files[i].getNumChannels(); ++n)
                  {
                      sum += input_files[i].samples[n][m];
                  }
                  input_files[i].samples[0][m] = sum / input_files[i].getNumChannels();
              }
              input_files[i].setNumChannels(1);
          }
      }
  }

  //resize all files to longest length
  for (size_t i = 0; i < input_files.size(); ++i)
      input_files[i].setNumSamplesPerChannel(int(longest_len * outputSamplerate));


  int slices = std::max((unsigned int)input_files.size(), minGridSz);
  AudioFile<float> output;
  output.setNumChannels(channelMode == 0 ? 2 : 1);
  output.setBitDepth(outputBitDepth);
  output.setSampleRate(outputSamplerate);
  output.setNumSamplesPerChannel(int(slices * longest_len * outputSamplerate));
  for (int ch = 0; ch < output.getNumChannels(); ++ch)
  {
      size_t output_iter = 0;
      for (size_t i = 0; i < input_files.size(); ++i)
      {
          for (int n = 0; n < input_files[i].getNumSamplesPerChannel(); ++n)
          {
              output.samples[ch][output_iter] = input_files[i].samples[ch][n];
              ++output_iter;
          }
      }
  }

  AudioFileFormat format = AudioFileFormat::Wave;
  if (output_path.back() != 'v' && output_path.back() != 'V') format = AudioFileFormat::Aiff;
  output.save(output_path, format);

  MessageBox(NULL, L"Sample grid file saved successfully.", L"Save", MB_OK);
}

void OpenFileDialog(HWND hwnd)
{
  OPENFILENAMEA ofn = { sizeof(ofn) };
  const size_t sz = 8192;
  CHAR path[sz] = {};
  ofn.hwndOwner = hwnd;
  ofn.lpstrFile = path;
  ofn.nMaxFile = ARRAYSIZE(path);
  ofn.lpstrDefExt = "wav";
  ofn.lpstrFilter = "WAV\0*.wav\0AIFF\0*.aiff;*.aif\0\0";
  ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  bool res = true;
  res = GetOpenFileNameA(&ofn);
  if (res == false) return;
  input_filenames.clear();
  unsigned int file_count = 0;
  CHAR* iter = path;
  std::vector<std::string> strs;
  while (iter < path + sz)
  {
    char* next_iter = std::find(iter, path + sz, '\0');
    if (next_iter == iter) break;
    strs.push_back(std::string(iter, next_iter - iter));
    iter = next_iter + 1;
  }
  if (strs.size() == 1) input_filenames = strs;
  else if (strs.size() > 1)
  {
    for (size_t i = 1; i < strs.size(); ++i)
    {
      input_filenames.push_back(strs[0] + '\\' + strs[i]);
    }
  }
  std::string count_text = std::to_string(input_filenames.size()) + " files selected";
  SetWindowTextA(GetDlgItem(hwnd, IDC_FILECOUNT), count_text.c_str());
}

void SaveFile(HWND hwnd)
{
  if (input_filenames.size() <= 0)
  {
    MessageBox(NULL, L"No input files selected.", L"Error", MB_OK);
    return;
  }
  OPENFILENAMEA ofn = { sizeof(ofn) };
  CHAR path[MAX_PATH] = {};
  ofn.hwndOwner = hwnd;
  ofn.lpstrFile = path;
  ofn.lpstrDefExt = "wav";
  ofn.Flags = OFN_OVERWRITEPROMPT;
  ofn.nMaxFile = ARRAYSIZE(path);
  ofn.lpstrFilter = "WAV\0*.wav\0AIFF\0*.aiff;*.aif\0\0";
  bool res = GetSaveFileNameA(&ofn);
  if (res == false) return;

  CHAR buf[32] = {};
  GetWindowTextA(GetDlgItem(hwnd, IDC_CHOOSEGRIDSIZE_INPUT), buf, sizeof(buf) / sizeof(char));
  minGridSz = std::max(std::atoi(buf), 0);
  channelMode = ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_CHANNELS));
  outputBitDepth = bitdepths[ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_BITDEPTH))];
  outputSamplerate = samplerates[ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_SAMPLERATE))];
  GetWindowTextA(GetDlgItem(hwnd, IDC_MAXLENGTH), buf, sizeof(buf) / sizeof(char));
  maxLength = (float)std::max(std::atof(buf), 0.0);
  ExportSampleGrid(hwnd, path);
}

// Message handler for about box.
INT_PTR CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        SetWindowTextA(GetDlgItem(hWnd, IDC_CHOOSEGRIDSIZE_INPUT), "0");
        SetWindowTextA(GetDlgItem(hWnd, IDC_MAXLENGTH), "0");
        for (auto iter : bitdepths)
            ComboBox_AddString(GetDlgItem(hWnd, IDC_BITDEPTH), std::to_wstring(iter).c_str()); 
        ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_BITDEPTH), 0);
        for (auto iter : samplerates)
            ComboBox_AddString(GetDlgItem(hWnd, IDC_SAMPLERATE), std::to_wstring(iter).c_str());
        ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_SAMPLERATE), 4);
        for (auto iter : channels)
            ComboBox_AddString(GetDlgItem(hWnd, IDC_CHANNELS), iter);
        ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_CHANNELS), 0);
        return (INT_PTR)TRUE;
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_COMMAND:
        // Parse the menu selections:
        switch (LOWORD(wParam))
        {
            case ID_EXIT:
              DestroyWindow(hWnd);
              break;
            case ID_SAVE:
              SaveFile(hWnd);
              break;
            case IDC_CHOOSEFILES:
              OpenFileDialog(hWnd);
              break;
            default:
              return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_QUIT:
        DestroyWindow(hWnd);
        break;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    }
    return (INT_PTR)FALSE;
}