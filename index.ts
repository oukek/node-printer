import bindings = require('bindings');
const printer = bindings('printer');

export interface PrinterInfo {
  name: string;
  papers: string[];
}

/**
 * 获取打印机列表以及支持纸张
 */
export function getPrinters(): PrinterInfo[] {
  return printer.getPrinters();
}

/**
 * 静默打印 PDF 或图片
 * @param printerName 打印机名称
 * @param paper 纸张类型
 * @param filePath 文件路径，PDF 或图片
 */
export function printFile(printerName: string, paper: string, filePath: string): boolean {
  return printer.printFile(printerName, paper, filePath);
}
