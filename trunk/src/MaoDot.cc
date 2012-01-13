//
// Copyright 2010 Google Inc.
//
// This program is free software; you can redistribute it and/or to
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

//
//  DOT - Print out the CFG of functions to dot[1] or vcg[2] files for
//  offline viewing.
//
//  Usage: ./mao --mao=DOT=options input.s
//      Pass specific options:
//          outputdir            - name of directory to place dot files.
//          include_instructions - include instructions/labels in dot file.
//
//  Example: Print out the CFG for main() in input.s to the directory
//         "~user/output/"
//  ./mao --mao=DOT=output_dir[~user/output/],apply_to_funcs=main input.s
//
//  The filename will always be the <functionname>.<extention>
//  By default, each node in the graph only shows the name of the label
//  starting the basic block. The actual instructions can be included
//  with the option include_instructions.
//
//  [1] http://www.graphviz.org/
//  [2] http://rw4.cs.uni-sb.de/~sander/html/gsvcg1.html

#include "Mao.h"  // Needs to be before MaoDefs.h since MaoDefs.h
                  // uses reg_entry which is defined in the unguarded
                  // i386-opc.h.
                  // (MaoUnit.h -> tc-i386.h -> opcodes/i386-opc.h)

namespace {

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(DOT, "Print a visual representation of the CFG.", 3) {
  OPTION_STR("output_dir", NULL,
             "Output directory (default = current directory)"),
  OPTION_BOOL("include_instructions", false, "Include instructions in output."),
  OPTION_STR("format", "dot",
             "Format of output. Supported formats are dot and vcg."),
};

class DotPass : public MaoFunctionPass {
 public:
  DotPass(MaoOptionMap *options, MaoUnit *mao, Function *function);
  bool Go();
 private:

  enum OutputFormats {
    kDot,
    kVcg,
    kInvalid,
  };

  // Print the CFG to filename.
  // Return false on error.
  bool PrintDot(const CFG *cfg, FILE *f);
  bool PrintVcg(const CFG *cfg, FILE *f);

  // Construct the filename (with path if specified by user) for the file.
  // Memory is allocated in the function and should be freed by the caller.
  char *GetFilenameFromFunction();

  // Return a string with can be printed in HTML.
  std::string MakeHTMLSafe(std::string s);

  enum OutputFormats output_format_;
  const char *output_dir_;
  bool include_instructions_;
};

DotPass::DotPass(MaoOptionMap *options, MaoUnit *mao, Function *function)
    : MaoFunctionPass("DOT", options, mao, function),
      output_dir_(GetOptionString("output_dir")),
      include_instructions_(GetOptionBool("include_instructions")) {
  std::string format = GetOptionString("format");
  if (format == "dot") {
    output_format_ = kDot;
  } else if (format == "vcg") {
    output_format_ = kVcg;
  } else {
    output_format_ = kInvalid;
  }
}

// Return the full filename for the output file.
// If the flag output_dir is given, include the path in the filename.
// The name of the file is constructed as <functionname>.<extention>
char *DotPass::GetFilenameFromFunction() {
  char *out_filename;
  // Calculate length of filename. Include null character and extension.
  int out_filename_length = function_->name().length() + 1 + 4;
  const char *path;

  // Determine the directory to use.
  if (output_dir_ == NULL) {
    path = "";
  } else {
    path = output_dir_;
    out_filename_length += strlen(output_dir_);
  }

  // Check if we need to add a directory separator.
  const char *separator_if_needed = "";
  if (strlen(path) > 0 && path[strlen(path)-1] != '/') {
    separator_if_needed = "/";
    ++out_filename_length;
  }

  // Allocate memory and generate the full filename.
  out_filename = static_cast<char *>(xmalloc(sizeof(char)
                                             * out_filename_length));
  const char *extension;
  switch (output_format_) {
    case kDot:
      extension = "dot";
      break;
    case kVcg:
      extension = "vcg";
      break;
    default:
      MAO_RASSERT(false);
      return NULL;  // Will never execute.
  }
  snprintf(out_filename, out_filename_length, "%s%s%s.%s",
           path,
           separator_if_needed,
           function_->name().c_str(),
           extension);

  return out_filename;
}

bool DotPass::Go() {
  // Make sure user has given a supported output format.
  if (output_format_ == kInvalid) {
    fprintf(stderr, "Not a valid output format. See help for DOT pass for "
            "list of supported formats.\n");
    return false;
  }

  CFG *cfg = CFG::GetCFG(unit_, function_);
  char *filename = GetFilenameFromFunction();
  MAO_ASSERT(filename != NULL);
  Trace(3, "Printing file %s", filename);

  FILE *f = fopen(filename, "w");
  if (f == NULL) {
    fprintf(stderr, "Error opening file %s for writing\n", filename);
    return false;
  }
  bool ret;
  switch (output_format_) {
    case kDot:
      ret = PrintDot(cfg, f);
      break;
    case kVcg:
      ret = PrintVcg(cfg, f);
      break;
    default:
      MAO_RASSERT(false);
      return NULL;  // Will never execute.
  }
  fclose(f);
  free(filename);
  return ret;
}


// Replace the following characters with HTML codes
// '<','>','\t'
// Note that spaces are preserved in the dot HTML
// language and thus &nbsp; are not needed:
// http://www.graphviz.org/doc/info/shapes.html
std::string  DotPass::MakeHTMLSafe(std::string s) {
  std::string::size_type pos;
  pos = 0;
  while ((pos = s.find("<", pos)) != std::string::npos) {
    s.replace(pos, 1, "&lt;");
    pos += 3;
  }
  pos = 0;
  while ((pos = s.find(">", pos)) != std::string::npos) {
    s.replace(pos, 1, "&gt;");
    pos += 3;
  }
  pos = 0;
  while ((pos = s.find("\t", pos)) != std::string::npos) {
    s.replace(pos, 1, "    ");
    pos += 4;
  }
  return s;
}

bool  DotPass::PrintDot(const CFG *cfg, FILE *f) {
  const char *html_table_start =
      "<TABLE  BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"0\">";
  const char *html_table_end = "</TABLE>";
  const char *html_header_start = "<TR><TD BGCOLOR=\"#FFFFDD\" "
                                          "BORDER=\"1\" "
                                          "ALIGN=\"CENTER\">";
  const char *html_header_end = "</TD></TR>";
  const char *html_row_start = "<TR><TD ALIGN=\"LEFT\">";
  const char *html_row_end = "</TD></TR>";

  fprintf(f, "digraph %s {\n", function_->name().c_str());

  FORALL_CFG_BB(cfg, it) {
    std::string table_contents = html_header_start
        + MakeHTMLSafe((*it)->label()) + html_header_end;

    if (include_instructions_) {
      FORALL_BB_ENTRY(it, entry) {
        if ( (*entry)->Type() == MaoEntry::INSTRUCTION ||
             (*entry)->Type() == MaoEntry::DIRECTIVE ||
             (*entry)->Type() == MaoEntry::LABEL) {
          std::string s;
          (*entry)->ToString(&s);
          table_contents += html_row_start + MakeHTMLSafe(s) + html_row_end;
        }
      }  // FORALL_BB_ENTRY
    }

    fprintf(f, "bb%d [ shape=\"box\" \n label=<%s%s%s>]\n",
            (*it)->id(),
            html_table_start,
            table_contents.c_str(),
            html_table_end);
    for (BasicBlock::ConstEdgeIterator eiter = (*it)->BeginOutEdges();
         eiter != (*it)->EndOutEdges(); ++eiter) {
      fprintf(f, "bb%d -> bb%d\n", (*eiter)->source()->id(),
              (*eiter)->dest()->id() );
    }
  }  // FORALL_CFG_BB
  fprintf(f, "}\n");
  return true;
}

bool  DotPass::PrintVcg(const CFG *cfg, FILE *f) {
  fprintf(f,
          "graph: { title: \"CFG\" \n"
          "splines: yes\n"
          "layoutalgorithm: dfs\n"
          "\n"
          "node.color: lightyellow\n"
          "node.textcolor: blue\n"
          "edge.arrowsize: 15\n");
  FORALL_CFG_BB(cfg, it) {
    fprintf(f, "node: { title: \"bb%d\" label: \"bb%d: %s\" %s",
            (*it)->id(), (*it)->id(), (*it)->label(),
            (*it)->id() < 2 ? "color: red" : "");
    fprintf(f, " info1: \"");

    FORALL_BB_ENTRY(it, entry) {
      if ( (*entry)->Type() == MaoEntry::INSTRUCTION ||
           (*entry)->Type() == MaoEntry::DIRECTIVE ||
           (*entry)->Type() == MaoEntry::LABEL) {
        std::string s;
        (*entry)->ToString(&s);
        // Escape strings for vcg.
        std::string::size_type pos = 0;
        while ((pos = s.find("\"", pos)) != std::string::npos) {
          s.replace(pos, 1, "\\\"");
          pos += 2;
        }
        fprintf(f, "%s", s.c_str());
      }
      fprintf(f, "\\n");
    }  // FORALL_BB_ENTRY

    fprintf(f, "\"}\n");
    for (BasicBlock::ConstEdgeIterator eiter = (*it)->BeginOutEdges();
         eiter != (*it)->EndOutEdges(); ++eiter) {
      fprintf(f, "edge: { sourcename: \"bb%d\" targetname: \"bb%d\" }\n",
              (*eiter)->source()->id(), (*eiter)->dest()->id() );
    }
  }  // FORALL_CFG_BB
  fprintf(f, "}\n");
  return true;
}


REGISTER_FUNC_PASS("DOT", DotPass)

}  // namespace
