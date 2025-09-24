#include <napi.h>
#include <string>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <winspool.h>
#include <GdiPlus.h>
#pragma comment(lib, "GdiPlus.lib")
#include "pdfium/include/fpdfview.h"
#include "pdfium/include/fpdf_doc.h"
#include "pdfium/include/fpdf_text.h"
#elif __linux__
#include <cups/cups.h>
#elif __APPLE__
#include <cups/cups.h>
#endif

namespace fs = std::filesystem;

// --------------------- 获取打印机列表 ---------------------
Napi::Array GetPrintersWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Array result = Napi::Array::New(env);

#ifdef _WIN32
  DWORD needed = 0, returned = 0;
  EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
               NULL, 2, NULL, 0, &needed, &returned);
  std::vector<BYTE> buffer(needed);
  if (EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                   NULL, 2, buffer.data(), needed, &needed, &returned)) {
    PRINTER_INFO_2* printerInfo = (PRINTER_INFO_2*)buffer.data();
    for (DWORD i = 0; i < returned; i++) {
      Napi::Object obj = Napi::Object::New(env);
      obj.Set("name", printerInfo[i].pPrinterName);

      DWORD paperCount = DeviceCapabilities(printerInfo[i].pPrinterName, NULL, DC_PAPERNAMES, NULL, NULL);
      Napi::Array papers = Napi::Array::New(env);
      if (paperCount > 0) {
        std::vector<char> names(paperCount * 64);
        DeviceCapabilities(printerInfo[i].pPrinterName, NULL, DC_PAPERNAMES, (LPSTR)names.data(), NULL);
        for (DWORD j = 0; j < paperCount; j++) {
          papers.Set(j, std::string(&names[j * 64]));
        }
      }
      obj.Set("papers", papers);
      result.Set(i, obj);
    }
  }

#elif __linux__ || __APPLE__
  int num_dests;
  cups_dest_t* dests;
  num_dests = cupsGetDests(&dests);
  for (int i = 0; i < num_dests; i++) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("name", dests[i].name);
    Napi::Array papers = Napi::Array::New(env);
    const char* ppdFile = cupsGetPPD(dests[i].name);
    if (ppdFile) {
      FILE* fp = fopen(ppdFile, "r");
      if (fp) {
        char line[256];
        int idx = 0;
        while (fgets(line, sizeof(line), fp)) {
          if (strncmp(line, "*PageSize", 9) == 0) {
            char paper[64];
            sscanf(line, "*PageSize %s", paper);
            papers.Set(idx++, std::string(paper));
          }
        }
        fclose(fp);
      }
    }
    obj.Set("papers", papers);
    result.Set(i, obj);
  }
  cupsFreeDests(num_dests, dests);
#endif

  return result;
}

// --------------------- Windows PDF 渲染 + GDI 打印 ---------------------
#ifdef _WIN32
bool PrintPDF_Windows(const std::string& printerName, const std::string& filePath) {
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

  FPDF_InitLibrary();
  FPDF_DOCUMENT pdfDoc = FPDF_LoadDocument(filePath.c_str(), NULL);
  if (!pdfDoc) return false;
  int pageCount = FPDF_GetPageCount(pdfDoc);

  DOCINFO di = { sizeof(DOCINFO), L"PDF Print Job", NULL };
  HDC hDC = CreateDC(NULL, printerName.c_str(), NULL, NULL);
  if (!hDC) return false;
  if (StartDoc(hDC, &di) <= 0) return false;

  for (int i = 0; i < pageCount; i++) {
    FPDF_PAGE page = FPDF_LoadPage(pdfDoc, i);
    if (!page) continue;

    int width = (int)FPDF_GetPageWidth(page);
    int height = (int)FPDF_GetPageHeight(page);

    HDC memDC = CreateCompatibleDC(hDC);
    HBITMAP hBmp = CreateCompatibleBitmap(hDC, width, height);
    SelectObject(memDC, hBmp);

    FPDF_RenderPageBitmap(hBmp, memDC, 0, 0, width, height, 0, 0);

    StartPage(hDC);
    BitBlt(hDC, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    EndPage(hDC);

    DeleteDC(memDC);
    DeleteObject(hBmp);
    FPDF_ClosePage(page);
  }

  EndDoc(hDC);
  DeleteDC(hDC);
  FPDF_CloseDocument(pdfDoc);
  FPDF_DestroyLibrary();
  Gdiplus::GdiplusShutdown(gdiplusToken);
  return true;
}

bool PrintImage_Windows(const std::string& printerName, const std::string& filePath) {
  HDC hDC = CreateDC(NULL, printerName.c_str(), NULL, NULL);
  if (!hDC) return false;

  DOCINFO di = { sizeof(DOCINFO), L"Image Print Job", NULL };
  if (StartDoc(hDC, &di) <= 0) return false;

  HBITMAP hBmp = (HBITMAP)LoadImageA(NULL, filePath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
  if (!hBmp) return false;

  HDC memDC = CreateCompatibleDC(hDC);
  SelectObject(memDC, hBmp);

  BITMAP bm;
  GetObject(hBmp, sizeof(bm), &bm);

  StartPage(hDC);
  BitBlt(hDC, 0, 0, bm.bmWidth, bm.bmHeight, memDC, 0, 0, SRCCOPY);
  EndPage(hDC);

  EndDoc(hDC);
  DeleteDC(memDC);
  DeleteObject(hBmp);
  DeleteDC(hDC);
  return true;
}
#endif

// --------------------- 打印文件 ---------------------
Napi::Boolean PrintFileWrapped(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 3) {
    Napi::TypeError::New(env, "Expected printerName, paper, filePath").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  std::string printerName = info[0].As<Napi::String>();
  std::string paper = info[1].As<Napi::String>();
  std::string filePath = info[2].As<Napi::String>();

#ifdef _WIN32
  fs::path p(filePath);
  bool success = false;
  if (p.extension() == ".pdf") success = PrintPDF_Windows(printerName, filePath);
  else success = PrintImage_Windows(printerName, filePath);
  return Napi::Boolean::New(env, success);

#elif __linux__ || __APPLE__
  std::string cmd = "lp -d " + printerName + " -o media=" + paper + " \"" + filePath + "\"";
  int ret = system(cmd.c_str());
  return Napi::Boolean::New(env, ret == 0);
#endif
}

// --------------------- 模块初始化 ---------------------
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getPrinters", Napi::Function::New(env, GetPrintersWrapped));
  exports.Set("printFile", Napi::Function::New(env, PrintFileWrapped));
  return exports;
}

NODE_API_MODULE(printer, Init)
