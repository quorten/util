/* This is a compatibility library so that Orly's Draw a Story can be
   run on Windows NT.  The technique is tricky: the solution is to
   compile a DLL with an empty object and only name the printer
   functions to be exported within the ".def" file.  That way, the
   static library "winspool.lib" will be linked into the DLL and the
   functions of that library will be exported.  */
