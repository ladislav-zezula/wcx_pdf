* PDF plugin for Total Commander *

What is a PDF?
---------------
See Wikipedia: https://cs.wikipedia.org/wiki/Portable_Document_Format

How to instal this plugin
-------------------------
1. Locate the wcx_pdf.zip file in Total Commander
2. Double-click on it with the mouse. Note that Ctrl+PgDown doesn't trigger the installation.
3. Total Commander will tell you that the archive contains a PDF packer plugin
   and asks whether you want to install it. Click on "Yes".
   Installation from a given ZIP only triggers once per Total Commander run -
   - if you decline by accident, restart Total Commander and retry.
4. Next, Total Commander asks where do you want to install the plugin.
   Just confirm by clicking "OK"
5. Total commander will then install the plugin
6. After the installation, a configuration dialog opens. By default, the extension is set to
   "pdf_dat" to avoid conflict between opening PDFs in PDF viewer and opening PDFs
   in Total Commander unpacker plugin.
   Just click "OK"
7. The plugin should now be fully operational. Try it by locating a PDF file
   and double-clicking it in Total Commander
8. Alternatively, you can press Ctrl+PageDown on a PDF file (regardless of its extension)

How to instal this plugin (the old way)
---------------------------------------
1. Create a folder under Total Commander plugins (TotalCmd\plugins\wcx\pdf)
2. Unpack the content of wcx_pdf.zip to that folder
3. In Total Commander, go to Configuration / Options ... / Plugins / Packer Plugins / Configure
4. Into the "All files with extension (ending with)" combo box, type "pdf"
5. Into the "Associate with" edit box, type the full path of the plugin
   (C:\Program Files\Totalcmd\plugins\wcx\pdf\pdf.wcx)
6. Confirm by clicking "OK".
7. The plugin should now be fully operational. Try it by locating a PDF file
   and double-clicking it in Total Commander


Files in the pack
-----------------
* pdf.wcx      - 32-bit Total Commander plugin
* pdf.wcx64    - 64-bit Total Commander plugin
* pluginst.inf - This file is recognized by Total Commander
* readme.txt   - This text file


 Authors
 -------
 The PDF plugin has been derived written by Ladik (http://www.zezula.net).
 Send any comments, suggestions and/or critics to zezula@volny.cz.

 Ladik
