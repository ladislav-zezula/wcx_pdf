### PDF Plugin for Total Commander
This plugin can be installed into Total Commander. It allows to view PDF files as if they were a ZIP file.

Because PDF files are documents, the plugin turns PDF objects into virtual files.

### Build Requirements
To build the PDF plugin, you need to have one of these build environments
* Visual Studio 202x
* WDK 6001
Also, the following tool is needed to be in your PATH:
* zip.exe (https://sourceforge.net/projects/infozip/files/)

1) Make a new directory, e.g. C:\Projects
```
md C:\Projects
cd C:\Projects
```

2) Clone the common library
```
git clone https://github.com/ladislav-zezula/Aaa.git
```

3) Clone and build the WCX_PDF plugin
```
git clone https://github.com/ladislav-zezula/wcx_pdf.git
cd wcx_pdf
make-msvc.bat
```
The installation package "wcx_pdf.zip" will be in the project directory.

4) Install the plugin.
 * Locate the wcx_pdf.zip file in Total Commander
 * Double-click on it with the mouse
 * Total Commander will tell you that the archive contains a PDF packer plugin
   and asks whether you want to install it. Click on "Yes".
 * Next, Total Commander asks where do you want to install the plugin.
   Just confirm by clicking "OK"
