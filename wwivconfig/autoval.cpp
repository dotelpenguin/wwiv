/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2020, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "localui/wwiv_curses.h"

#include "core/strings.h"
#include "core/wwivport.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "sdk/vardec.h"
#include "wwivconfig/utility.h"
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static string create_autoval_line(Config& config, int n) {
  char ar[20], dar[20], r[20];
  const auto v = config.auto_val(n);
  string res_str = restrict_string;
  for (int8_t i = 0; i <= 15; i++) {
    if (v.ar & (1 << i)) {
      ar[i] = 'A' + i;
    } else {
      ar[i] = 32;
    }
    if (v.dar & (1 << i)) {
      dar[i] = 'A' + i;
    } else {
      dar[i] = 32;
    }
    if (v.restrict & (1 << i)) {
      r[i] = res_str[i];
    } else {
      r[i] = 32;
    }
  }
  r[16] = 0;
  ar[16] = 0;
  dar[16] = 0;
  const auto key = fmt::format("ALT-F{}", n + 1);
  return fmt::sprintf("%-7s  %3d  %3d  %16s  %16s  %20s", key, v.sl, v.dsl, ar, dar, r);
}

static void edit_autoval(Config& config, int n) {
  constexpr int LABEL1_POSTITION = 2;
  constexpr int LABEL1_WIDTH = 14;
  constexpr int COL1_POSITION = LABEL1_POSTITION + LABEL1_WIDTH + 1;

  valrec v = config.auto_val(n);
  EditItems items{};
  items.add_items({
      new NumberEditItem<uint8_t>(COL1_POSITION, 1, &v.sl),
      new NumberEditItem<uint8_t>(COL1_POSITION, 2, &v.dsl),
      new ArEditItem(COL1_POSITION, 3, &v.ar),
      new ArEditItem(COL1_POSITION, 4, &v.dar),
      new RestrictionsEditItem(COL1_POSITION, 5, &v.restrict),
  });
  int y = 1;
  items.add_labels({new Label(LABEL1_POSTITION, y++, LABEL1_WIDTH, "SL:"), 
                    new Label(LABEL1_POSTITION, y++, LABEL1_WIDTH, "DSL:"),
                    new Label(LABEL1_POSTITION, y++, LABEL1_WIDTH, "AR:"),
                    new Label(LABEL1_POSTITION, y++, LABEL1_WIDTH, "DAR:"),
                    new Label(LABEL1_POSTITION, y++, LABEL1_WIDTH, "Restrictions:")
  });
  items.Run(fmt::format("Auto-validation data for: Alt-F{}", n + 1));
  config.auto_val(n, v);
}

void autoval_levs(wwiv::sdk::Config& config) {
  bool done = false;
  do {
    curses_out->Cls(ACS_CKBOARD);
    vector<ListBoxItem> items;
    for (int i = 0; i < 10; i++) {
      items.emplace_back(create_autoval_line(config, i));
    }
    CursesWindow* window(curses_out->window());
    ListBox list(window, "Select AutoVal", items);

    list.selection_returns_hotkey(true);
    list.set_additional_hotkeys("DI");
    list.set_help_items({{"Esc", "Exit"}, {"Enter", "Edit"}});
    auto result = list.Run();

    if (result.type == ListBoxResultType::HOTKEY) {
    } else if (result.type == ListBoxResultType::SELECTION) {
      edit_autoval(config, result.selected);
    } else if (result.type == ListBoxResultType::NO_SELECTION) {
      done = true;
    }
  } while (!done);
  config.Save();
}
