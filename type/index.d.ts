export interface PrinterInfo {
  name: string;
  papers: string[];
}

export declare function getPrinters(): PrinterInfo[];

export declare function printFile(printerName: string, paper: string, filePath: string): boolean;
