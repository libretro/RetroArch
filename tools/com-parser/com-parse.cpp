/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <peglib.h>
#include <fstream>
#include <sstream>
#include <cstring>

using namespace peg;
using namespace std;

template <typename Annotation>
AstBase<Annotation> AstBase<Annotation>::empty = AstBase<Annotation>("", 0, 0, "", string(""));

//bool use_typedefs = false;
bool use_typedefs = true;

//const char* prefix_seperator = "_";
const char* prefix_seperator = "";

vector<string> ignored_fn_prefixes_list =
{
   "OM",
   "IA",
   "RS",
};

vector<string> fn_prefixes_list =
{
   "OM",
   "IA",
   "RS",
   "VS",
   "PS",
   "GS",
   "DS",
   "CS",
};

bool move_fn_prefix_after_action = true;

vector<string> ignored_types_list =
{
   "AsyncIUnknown",
   "IMarshal",
   "IMalloc",
   "IEnumString",
   "ISequentialStream",
   "IStream",
   "IRpcChannelBuffer",
   "IAsyncRpcChannelBuffer",
   "IRpcProxyBuffer",
   "IServerSecurity",
   "IRpcOptions",
   "IGlobalOptions",
   "ISurrogate",
   "ISynchronize",
   "ISynchronizeEvent",
   "IDataObject",
   "IDataAdviseHolder",
   "IDirectWriterLock",
   "IDummyHICONIncluder",
   "IDispatch",
   "IDropSource",
   "IDropTarget",
   "IDataFilter",
   "IDropSourceNotify",
};

const char* required_prefix = "ID";

vector<string> ignored_functions_list =
{
   "Release",
   "QueryInterface",
   "AddRef",
   "GetParent",
   "GetAdapter",
   "GetDevice",
   "GetDesc",
   "GetType",
   "SetName",
   "SetPrivateDataInterface",
   "SetPrivateData",
   "GetPrivateData",

   "OpenSharedHandle",
   "CreateSharedHandle",
   "OpenSharedHandleByName",

   "SetTrackingOptions",
};

vector<string> overloaded_list =
{
   "Release",
   "QueryInterface",
   "AddRef",
   "GetParent",
   "GetAdapter",
   "GetDesc",
   "GetType",
   "SetName",
   "SetPrivateDataInterface",
   "SetPrivateData",
   "GetPrivateData",
   "Map",
   "Unmap",
   "Reset",
   "Signal",
   "BeginEvent",
   "EndEvent",
   "SetMarker",
   "AssertResourceState",
   "GetFeatureMask",
   "SetFeatureMask",
   "GetFrameStatistics",
   "Close",
   "SetEvictionPriority",
   "GetEvictionPriority",
   "GetResource",
   "GetDataSize",
   "GetContextFlags",
   "GetCertificate",
   "GetCertificateSize",
   "Begin",
   "End",
   "GetData",

   "CopySubresourceRegion",
   "CreateRenderTargetView",
   "CreateShaderResourceView",
};

vector<string> action_list =
{
   "Release",
   "Get",
   "Set",
   "Enum",
   "Get",
   "Query",
   "Add",
   "Signal",
   "Begin",
   "End",
   "Assert",
   "Close",
   "Map",
   "Unmap"
};

vector<string> derived_types_list =
{
   "ID3D12Device",
   "ID3D12Debug",
   "ID3D12DebugDevice",
   "ID3D12DebugCommandList",
   "IDXGIFactory1",
   "IDXGIAdapter1",
   "IDXGISurface1",
   "IDXGISwapChain3",
   "IDXGIOutput",
   "IDXGIDevice",
};

vector<string> base_objects_list =
{
   "IUnknown",
   "ID3D12Object",
//   "ID3D12Resource",
   "IDXGIObject",
   "IDXGIResource",
//   "ID3D11Resource",
//   "ID3D10Resource",
};

//string insert_name(const char* fname, const char* name)
string insert_name(const string& fname, const string& name)
{
   string str;
   for(string action: action_list)
   {
      if(!strncmp(fname.c_str(), action.c_str(), action.length()))
      {
         if(name.length() == 2 && name[1] == 'S')
         {
             if(!strncmp(fname.c_str() + action.length(), "Shader", strlen("Shader")))
               return action + name[0] + (fname.c_str() + action.length());
            else
               return action + name[0] + "Shader" + (fname.c_str() + action.length());
         }
         else
            return action + name + (fname.c_str() + action.length());
      }
   }
   return string(fname) + name;

}
//string insert_name(string fname, string name)
//{
//   return insert_name(fname.c_str(), name.c_str());
//}

string get_derived_basename(const char *name)
{
   string str = name;
   if(::isdigit(str.back()))
      str.pop_back();

   return str;
}

string get_derived_basename(string name)
{
   return get_derived_basename(name.c_str());
}

string get_prefix(const char *type)
{
   string prefix;

   if(*type != 'I')
      return type;

   type++;

   while (::isalnum(*type) && !::islower(*type))
      prefix.push_back(*type++);

   prefix.pop_back();

   return prefix;
}

string get_prefix(const string &type)
{
   return get_prefix(type.c_str());
}

string clean_name(const char *name)
{
   string str;

   if (name[0] == 'p' && name[1] == 'p' && name[2] == 'v' && ::isupper(name[3]))
      name += 3;
   if (name[0] == 'p' && name[1] == 'p' && ::isupper(name[2]))
      name += 2;
   else if ((*name == 'p' || *name == 'b') && ::isupper(name[1]))
      name ++;

   bool was_upper = false;
   if (*name)
   {
      if (::isupper(*name))
      {
         was_upper = true;
         str = (char)::tolower(*name);
      }
      else
         str = *name;
   }

   while (*++name)
   {
      if (::isupper(*name))
      {
         if(!was_upper && !::isdigit(str.back()))
            str = str + '_';

         was_upper = true;
         str += (char)::tolower(*name);
      }
      else
      {
         was_upper = false;
         str += *name;
      }
   }

   if(str == "enum")
      str = "enumerator";

   return str;
}

struct name_pair{
   string name;
   string original;
};

class ComFunction
{
public:
   struct param_t
   {
      name_pair type;
      name_pair ptr;
      name_pair name;
      int array_size = 0;
      bool const_ = false;
      bool base = false;
      bool derived = false;
   };
   param_t this_;
   vector<param_t> params;
   string riid;

   string name;
   string original_name;
   string return_type;
   string prefix;

   bool overloaded = false;
   bool ignored = false;
   bool preferred = false;

   ComFunction(Ast node)
   {
      original_name = node["FunctionName"];
      return_type = node["ReturnType"];

      for (auto param_ : node["Parameters"].nodes)
      {
         Ast &param = *param_;
         param_t p;
         p.type.original = param["Type"]["Name"];
         p.const_ = param["Type"]["TypeQualifier"] == "const";

         if(p.type.original == "REFIID")
         {
            riid = string("I") + prefix;
            continue;
         }
         if(!riid.empty())
         {
            if(param["Name"][2] == 'v')
               riid += param["Name"] + 3;
            else
               riid += param["Name"] + 2;
            break;
         }

         p.ptr.original  = param["Type"]["Pointer"];
         p.ptr.name      = p.ptr.original;

         if(use_typedefs &&
            (prefix.empty() || (p.type.original[0] == 'I' && !strncmp(p.type.original.c_str() + 1, prefix.c_str(), prefix.length())))
            )
         {
            p.type.name = p.type.original.c_str() + 1;
            if(::isdigit(p.type.name.back()))
               p.type.name.pop_back();

            if(!p.ptr.name.empty())
               p.ptr.name.pop_back();
         }
         else
         {
            p.type.name = p.type.original;
            p.ptr.name = p.ptr.name;
         }

         if (param.name == "This")
         {
            prefix = get_prefix(p.type.original);
            p.name.original = param["Type"]["Name"] + 1 + prefix.length();
            p.name.name = clean_name(p.name.original.c_str());
            if(::isdigit(p.name.name.back()))
               p.name.name.pop_back();

            this_ = p;
         }
         else
         {
            p.name.original = param["Name"];
            p.name.name = clean_name(p.name.original.c_str());
            p.array_size = ::strtoul(param["ArraySize"], NULL, 10);
            params.push_back(p);
         }

      }

      if(original_name == "GetCPUDescriptorHandleForHeapStart")
      {
         param_t p;
         p.type.name = p.type.original = return_type;
         p.ptr.name = p.ptr.original = "*";
         return_type = "void";
         p.name.name = p.name.original = "out";
         params.push_back(p);
      }

      for(string str : ignored_functions_list)
         if(original_name == str)
            ignored = true;

      for(string str : ignored_types_list)
         if(get_derived_basename(this_.type.original) == get_derived_basename(str))
            ignored = true;

      if(required_prefix)
         if(strncmp(this_.type.original.c_str(), required_prefix, strlen(required_prefix)))
            ignored = true;

      for(string str : derived_types_list)
      {
         if(get_derived_basename(this_.type.original) == get_derived_basename(str))
         {
            this_.derived = true;
            if(this_.type.original != str)
               ignored = true;
         }
         for (param_t &param : params)
            if(get_derived_basename(param.type.original) == get_derived_basename(str))
            {
               if(param.type.original != str)
                  param.derived = true;
               else
               {
//                  cout << param.type.original << endl;
                  preferred = true;
               }
            }
      }

      for(string str : overloaded_list)
         if(original_name == str)
            overloaded = true;

      for(string str : base_objects_list)
      {
         if(this_.type.original == str)
            this_.base = true;
         for (param_t &param : params)
            if(param.type.original == str)
               param.base = true;

      }

      name = prefix + prefix_seperator;

      if(overloaded && !this_.base)
      {
         name += insert_name(original_name, this_.name.original);
         if(::isdigit(name.back()))
            name.pop_back();
      }
      else
      {
         string fn_prefix;
         for (string& str : ignored_fn_prefixes_list)
            if(!strncmp(original_name.c_str(), str.c_str(), str.length()))
            {
               if(::isupper(original_name[str.length()]))
                  fn_prefix = str;
               break;
            }
         if(!fn_prefix.empty())
         {
            name += original_name.c_str() + fn_prefix.length();
         }
         else
         {
            for (string str : fn_prefixes_list)
               if(!strncmp(original_name.c_str(), str.c_str(), str.length()))
               {
                  if(::isupper(original_name[str.length()]))
                     fn_prefix = str;
                  break;
               }
            if(!fn_prefix.empty())
               name += insert_name(original_name.c_str() + fn_prefix.length(), fn_prefix.c_str());
            else
               name += original_name;
         }
      }

//      if(original_name == "CreateCommandQueue" && ignored)
//         cout << "ignored CreateCommandQueue !!" << __LINE__ << endl;

   }
   string str()
   {
      stringstream out;
      out << "static inline " << return_type << " " << name << "(";

      if(this_.base)
         out << "void*";
      else
         out << this_.type.name << this_.ptr.name;

      out << " " << this_.name.name;

      for (param_t &param : params)
      {
         out << ", ";

         if(param.const_)
            out << "const ";

         if(param.base)
            out << "void*";
         else
            out << param.type.name << param.ptr.name;

         out << " " << param.name.name;
         if(param.array_size)
            out << '[' << param.array_size << ']';

      }
      if(!riid.empty())
         out << ", " << riid << "** out";

      out << ")\n{\n";
      out << "   ";

      if (return_type != "void")
         out << "return ";

      if(original_name == "GetCPUDescriptorHandleForHeapStart")
         out << "((void (STDMETHODCALLTYPE *)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*))\n         ";

      if(this_.base)
         out << "((" << this_.type.original << this_.ptr.original << ')' << this_.name.name << ')';
      else
         out << this_.name.name;

      out << "->lpVtbl->" << original_name;

      if(original_name == "GetCPUDescriptorHandleForHeapStart")
         out << ')';

      out << '(' << this_.name.name;

      for (param_t param : params)
      {
         out << ", ";

         if(param.base || param.derived)
            out << '(' << param.type.original << param.ptr.original << ')';

         out << param.name.name;
      }

      if(!riid.empty())
         out << ", uuidof(" << riid << "), (void**)out";

      out << ");\n";
      out << "}\n";
      return out.str();
   }

};
class ComHeader
{
public:
   shared_ptr<Ast> ast;
   vector<ComFunction> functions;
   vector<name_pair> types;
   ComHeader(const char *filename)
   {
      string header;
      {
         ifstream fs(filename);
#if 0
         stringstream ss;
         ss << fs.rdbuf();
         header = ss.str();
#else
         while (!fs.eof())
         {
            char line[4096];
            fs.getline(line, sizeof(line));
            char* str = line;
            while (*str && ::isspace(*str))
               str++;
            if (*str && !strncmp(str, "typedef struct ", strlen("typedef struct ")))
            {
               if(*str && strstr(str, "Vtbl"))
               {
                  header += str;
                  header.push_back('\n');
                  while(*line)
                  {
                     fs.getline(line, sizeof(line));
                     str = line;
                     while (*str && ::isspace(*str))
                        str++;
                     if(*str)
                     {
                        header += str;
                        header.push_back('\n');
                     }
                     if(*str == '}')
                        break;
                  }
               }
            }
         }
#endif
      }
      {
         ofstream out("test.cpp.h");
         out << header;
         out.close();
      }

      string header_grammar;
      {
         ifstream fs("grammar.txt");
         stringstream ss;
         ss << fs.rdbuf();
         header_grammar = ss.str();
      }

      parser parser;
      parser.log = [&](size_t ln, size_t col, const string & msg)
      {
         cout << "Error parsing grammar:" << ln << ":" << col << ": " << msg << endl;
      };

      if (!parser.load_grammar(header_grammar.c_str()))
      {
         cout << "Failed to load grammar" << endl;
         return;
      }

      parser.log = [&](size_t ln, size_t col, const string & msg)
      {
         cout << filename << ":" << ln << ":" << col << ": " << msg << endl;
      };

      parser.enable_ast();

      if (!parser.parse_n(header.c_str(), header.size(), ast))
      {
         cout << "Error parsing header file: " << filename << endl;
         return;
      }

      ast = AstOptimizer(false).optimize(ast);

      if (ast->name != "Program")
      {
         cout << "Expected root node to be Program, not" << ast->name << endl;
         return;
      }

      for (shared_ptr<Ast> node_ : ast->nodes)
      {
         Ast node = *node_;

         if (node.name == "Function")
            functions.push_back(node);
         else if (node.name == "Line")
         {

         }
         else if (node.name == "EndOfFile")
            break;
         else
         {
//           cout << "Unexcpected node " << node->name << endl;
         }
      }

      for(ComFunction& fn : functions)
      {
         if(!fn.ignored && fn.preferred && ::isdigit(fn.name.back()))
         {
            fn.name.pop_back();
            for(ComFunction& fn2 : functions)
               if(&fn != &fn2 && !fn2.ignored && get_derived_basename(fn.original_name) == get_derived_basename(fn2.original_name))
               {
//                  cout << &fn << &fn2 << fn.original_name << " " << fn2.original_name << endl;
//                  assert(fn2.preferred == false);
                  fn2.ignored = true;
//                  if(fn2.original_name == "CreateCommandQueue" && fn2.ignored)
//                     cout << "ignored CreateCommandQueue !!" << __LINE__ << endl;
               }
         }
      }

      for(ComFunction& fn : functions)
      {
         if(!fn.ignored)
         {
            bool known = false;
            for(name_pair& known_type:types)
               if (fn.this_.type.original == known_type.original)
               {
                  known = true;
                  break;
               }

            if(!known)
               types.push_back(fn.this_.type);
         }
      }
   }
   ComHeader(shared_ptr<Ast> ast_)
   {

   }
   string str()
   {
      stringstream out;
      int indent = 0;
      for(name_pair known_type:types)
         if (indent < known_type.original.length())
            indent = known_type.original.length();

      for(name_pair known_type:types)
      {
         out << "typedef " << known_type.original << '*' << string(indent + 1 - known_type.original.length(), ' ') << known_type.name << ";\n";
      }
      out << "\n\n";
      for (ComFunction &fn : functions)
         if(!fn.ignored)
            out << fn.str();
      return out.str();
   }

};

template<class _Elem, class _Traits>
basic_ostream<_Elem, _Traits> &operator << (basic_ostream<_Elem, _Traits> &ios, ComHeader &header)
{
   return ios << header.str();
}

int main(int argc, const char **argv)
{
   const char *header_fn = argv[1];
   ComHeader header(header_fn);

   if (header.functions.empty())
      return 1;

   if (argc > 2)
      cout << ast_to_s(header.ast) << flush;

   ofstream out("test.h");
   const char* basename = strrchr(argv[1], '/');
   if(!basename)
      basename = strrchr(argv[1], '\\');
   if(basename)
      basename++;
   if(!basename)
      basename = argv[1];

#if 1
   out << "\n#include <" << basename << ">\n\n";
#else
   out << "\n";
   out << "#include <d3d12.h>\n";
   out << "#include <dxgi1_5.h>\n";
   out << "#include <d3dcompiler.h>\n\n";
#endif

   out << header;
   out.close();

   return 0;
}
