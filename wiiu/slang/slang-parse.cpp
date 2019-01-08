
#include <iostream>
#include <peglib.h>
#include <fstream>
#include <sstream>

int main(int argc, const char** argv)
{
   const char* slang_fn = NULL;
   const char* vs_asm_fn = NULL;
   const char* ps_asm_fn = NULL;
   const char* vs_out_fn = NULL;
   const char* ps_out_fn = NULL;
   bool verbose = false;

   for(int i = 1; i < argc - 1; i++)
   {
      if(!strcmp(argv[i], "--slang"))
         slang_fn = argv[++i];
      else if(!strcmp(argv[i], "--vsource"))
         vs_asm_fn = argv[++i];
      else if(!strcmp(argv[i], "--psource"))
         ps_asm_fn = argv[++i];
      else if(!strcmp(argv[i], "--vsh"))
         vs_out_fn = argv[++i];
      else if(!strcmp(argv[i], "--psh"))
         ps_out_fn = argv[++i];
      else if(!strcmp(argv[i], "--verbose"))
         verbose = true;
   }

   if(!slang_fn || !vs_out_fn || !ps_out_fn || (!vs_asm_fn && ps_asm_fn) || (vs_asm_fn && !ps_asm_fn))
   {
      printf("Usage :\n");
      printf("%s --slang <slang input> --vsh <vsh output> --psh <psh output>\n", argv[0]);
      printf("%s --slang <slang input> --vsource <vsh asm input> --psource <psh asm input> --vsh <vsh output> --psh <psh output>\n", argv[0]);
   }

   std::string slang;
   {
      std::ifstream fs(slang_fn);
      std::stringstream ss;
      ss << fs.rdbuf();
      slang = ss.str();
   }

   std::string slang_grammar;
   {
      std::ifstream fs("grammar.txt");
      std::stringstream ss;
      ss << fs.rdbuf();
      slang_grammar = ss.str();
   }

   peg::parser parser;
   parser.log = [&](size_t ln, size_t col, const std::string &msg) {
      std::cout << "Error parsing grammar:" << ln << ":" << col << ": " << msg << std::endl;
   };

   if (!parser.load_grammar(slang_grammar.c_str())) {
      std::cout << "Failed to load grammar" << std::endl;
      return 1;
   }

   parser.log = [&](size_t ln, size_t col, const std::string &msg) {
      std::cout << slang_fn << ":" << ln << ":" << col << ": " << msg << std::endl;
   };

   parser.enable_ast();

   std::shared_ptr<peg::Ast> ast;
   if (!parser.parse_n(slang.c_str(), slang.size(), ast)) {
      std::cout << "Error parsing slang file: " << slang_fn << std::endl;
      return 1;
   }
   ast = peg::AstOptimizer(false).optimize(ast);
   if(verbose)
      std::cout << peg::ast_to_s(ast) << std::flush;

   if (ast->name != "Program") {
      std::cout << "Expected root node to be Program, not" << ast->name << std::endl;
      return 1;
   }

   std::stringstream common;
   std::stringstream vs;
   std::stringstream ps;

   std::stringstream* out = &common;

   common << "#version 150\n";
   common << "#define float2 vec2\n";
   common << "#define float3 vec3\n";
   common << "#define float4 vec4\n";
   for (std::shared_ptr<peg::Ast> &node : ast->nodes) {
      if (node->name == "Version") {
         /* do nothing */
      } else if (node->name == "UniformBlock") {
         int location = 0;
         int binding = 0;
         int set = 0;
         int std_val = 0;
         bool push_constant = false;
         std::string struct_name;
         std::string name;
         struct member_type
         {
            std::string type;
            std::string name;
         };
         std::vector<member_type> members;

         for (std::shared_ptr<peg::Ast> &child : node->nodes) {
            if (child->name == "Layout") {
               for (std::shared_ptr<peg::Ast> &layout : child->nodes) {
                  if (layout->name == "PushConstant") {
                     push_constant = true;
                  } else if (layout->name == "Location") {
                     location = std::stoul(layout->token);
                  } else if (layout->name == "Binding") {
                     binding = std::stoul(layout->token);
                  } else if (layout->name == "Set") {
                     set = std::stoul(layout->token);
                  } else if (layout->name == "Std") {
                     std_val = std::stoul(layout->token);
                  }
               }
            } else if (child->name == "StructName") {
               struct_name = child->token;
            } else if (child->name == "Name") {
               name = child->token;
            } else if (child->name == "Member") {
               member_type new_member;
               for (std::shared_ptr<peg::Ast> &member : child->nodes) {
                  if (member->name == "Type") {
                     new_member.type = member->token;
                  } else if (member->name == "Name") {
                     new_member.name = member->token;
                  }
               }
               members.push_back(new_member);
            }
         }

#if 0
         *out << "layout(location = " << (push_constant? "1" : "0");
         if(std_val)
            *out  << ", std" << std_val;
         *out  << ") ";
#else
         if(std_val)
            *out  << "layout(std" << std_val << ") ";
#endif
         *out << "uniform " << struct_name << "\n{\n";
         for(member_type &member : members) {
            *out  << "   " << member.type << " " << member.name << ";\n";
         }
         *out  << "}" << name << ";";

      } else if (node->name == "Declaration") {
         int location = 0;
         int binding = 0;
         int set = 0;
         std::string qualifier;
         std::string type;
         std::string name;
         int array_size = 0;
         bool has_layout = false;

         for (std::shared_ptr<peg::Ast> &child : node->nodes) {
            if (child->name == "Layout") {
               has_layout = true;
               for (std::shared_ptr<peg::Ast> &layout : child->nodes) {
                  if (layout->name == "Location") {
                     location = std::stoul(layout->token);
                  } else if (layout->name == "Binding") {
                     binding = std::stoul(layout->token);
                  } else if (layout->name == "Set") {
                     set = std::stoul(layout->token);
                  }
               }
            } else if (child->name == "Qualifier") {
               qualifier = child->token;
            } else if (child->name == "Type") {
               type = child->token;
            } else if (child->name == "Name") {
               name = child->token;
            }else if (child->name == "ArraySize") {
               array_size = std::stoul(child->token);
            }
         }
         if(has_layout && type != "sampler2D")
         {
            *out << "layout(location = " << location << ") ";
         }

         *out << qualifier << " " << type << " " << name;
         if(array_size)
            *out << "[" << array_size << "]";
         *out << ";";

      } else if (node->name == "ConstArray") {
         std::string type;
         std::string name;
         int array_size = 0;
         std::vector<std::shared_ptr<peg::Ast>>* array;

         for (std::shared_ptr<peg::Ast> &child : node->nodes) {
            if (child->name == "Type") {
               type = child->token;
            } else if (child->name == "Name") {
               name = child->token;
            } else if (child->name == "ArraySize") {
               array_size = std::stoul(child->token);
            } else if (child->name == "Array") {
               array = &child->nodes;
            }
         }
         *out << type << " " << name;
         if(array_size)
            *out << "[" << array_size << "]";
         *out << " = " << type;
         if(array_size)
            *out << "[" << array_size << "]";
         *out << "(";
         for (std::shared_ptr<peg::Ast> &item : *array) {
            if(array_size)
               *out << type << "(";
            *out << item->token;
            if(array_size)
               *out << ")";
            if(item != array->back())
               *out << ", ";
         }
         *out << ");";

      } else if (node->name == "Stage") {
         std::string stage = node->token;
         std::transform(stage.begin(), stage.end(), stage.begin(), ::toupper);
         if(stage == "VERTEX")
            out = &vs;
         else
            out = &ps;
      } else if (node->name == "Indent"){
         *out << node->token;
      } else if (node->name == "Line"){
         *out << node->token << std::endl;
      } else {
         std::cout << "Unexcpected node " << node->name << std::endl;
      }
   }

   {
      std::ofstream fs(vs_out_fn);
      fs << common.str();
      fs << vs.str();
   }

   {
      std::ofstream fs(ps_out_fn);
      fs << common.str();
      fs << ps.str();
   }

   return 0;
}
