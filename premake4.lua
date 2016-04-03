solution "WaveScribe"
   configurations { "Debug", "Release" }

   --SchifraDir = "/Users/lokno/Documents/Libraries/schifra-master/"
   --STBDir  = "/Users/lokno/Documents/Libraries/stb-master/"

   SchifraDir = "E:/libraries/schifra/"
   STBDir     = "C:/Users/Lokno/Documents/Projects/stb/"

   project "WaveScribe"
      kind "ConsoleApp"
      language "C++"

      files { STBDir  .. "stb_image.h",
              STBDir  .. "stb_image_write.h",
              "dwt.h",
              "dwt97.c",
              "wavescribe.cpp"
            }
 
      configuration { "Debug", "macosx" }
         defines { "_DEBUG","DEBUG" }
         includedirs { "/usr/local/include", SchifraDir, STBDir }
         libdirs { "/usr/local/lib" }
         links { }
         flags { "Symbols" }
 
      configuration { "Release", "macosx" }
         defines { }
         includedirs { "/usr/local/include", SchifraDir, STBDir }
         libdirs { "/usr/local/lib" }
         links { }
         flags { "Optimize" }

      configuration { "Debug", "windows" }
         targetdir  "bin/Debug"
         defines { "_DEBUG","DEBUG" }
         includedirs { SchifraDir, STBDir }
         libdirs { }
         links { }
         flags { "Symbols", "Unicode", "StaticRuntime" }

      configuration { "Release", "windows" }
         targetdir  "bin/Release"
         defines { "WIN32","_WINDOWS","_UNICODE","UNICODE" }
         includedirs { SchifraDir, STBDir }
         libdirs {  }
         links { }
         flags { "Optimize", "Unicode", "StaticRuntime" }
