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

#include "core/file.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/input.h"
#include "localui/wwiv_curses.h"
#include "sdk/filenames.h"
#include "wwivconfig/utility.h"
#include <cstdlib>
#include <memory>
#include <string>

static constexpr int MAX_SUBS_DIRS = 4096;

using std::unique_ptr;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

template<typename T>
static auto input_number(CursesWindow* window, int max_digits) -> T {
  string s;
  editline(window, &s, max_digits, EditLineMode::NUM_ONLY, "");
  if (s.empty()) {
    return 0;
  }
  try {
    auto num = std::stoi(s);
    return static_cast<T>(num);
  } catch (const std::logic_error&) { 
    // No conversion possible.
    return 0;
  }
}

static void convert_to(CursesWindow* window, uint16_t num_subs, uint16_t num_dirs,
                       Config& config) {
  int l1, l2, l3;

  if (num_subs % 32) {
    num_subs = (num_subs / 32 + 1) * 32;
  }
  if (num_dirs % 32) {
    num_dirs = (num_dirs / 32 + 1) * 32;
  }

  if (num_subs < 32) {
    num_subs = 32;
  }
  if (num_dirs < 32) {
    num_dirs = 32;
  }

  if (num_subs > MAX_SUBS_DIRS) {
    num_subs = MAX_SUBS_DIRS;
  }
  if (num_dirs > MAX_SUBS_DIRS) {
    num_dirs = MAX_SUBS_DIRS;
  }

  const auto nqscn_len =
      static_cast<uint16_t>(4 * (1 + num_subs + ((num_subs + 31) / 32) + ((num_dirs + 31) / 32)));
  auto nqsc = static_cast<uint32_t *>(malloc(nqscn_len));
  wwiv::core::ScopeExit free_nqsc([&]() { free(nqsc); nqsc = nullptr; });
  if (!nqsc) {
    return;
  }
  memset(nqsc, 0, nqscn_len);

  uint32_t* nqsc_n = nqsc + 1;
  uint32_t* nqsc_q = nqsc_n + ((num_dirs + 31) / 32);
  uint32_t* nqsc_p = nqsc_q + ((num_subs + 31) / 32);

  memset(nqsc_n, 0xff, ((num_dirs + 31) / 32) * 4);
  memset(nqsc_q, 0xff, ((num_subs + 31) / 32) * 4);

  auto oqsc = static_cast<uint32_t *>(malloc(config.qscn_len()));
  wwiv::core::ScopeExit free_oqsc([&]() { free(oqsc); oqsc = nullptr; });
  if (!oqsc) {
    messagebox(window, fmt::format("Could not allocate {} bytes for old quickscan rec\n",
                                    config.qscn_len()));
    return;
  }
  memset(oqsc, 0, config.qscn_len());

  const auto oqsc_n = oqsc + 1;
  const auto oqsc_q = oqsc_n + ((config.max_dirs() + 31) / 32);
  const auto oqsc_p = oqsc_q + ((config.max_subs() + 31) / 32);

  if (num_dirs < config.max_dirs()) {
    l1 = ((num_dirs + 31) / 32) * 4;
  } else {
    l1 = ((config.max_dirs() + 31) / 32) * 4;
  }

  if (num_subs < config.max_subs()) {
    l2 = ((num_subs + 31) / 32) * 4;
    l3 = num_subs * 4;
  } else {
    l2 = ((config.max_subs() + 31) / 32) * 4;
    l3 = config.max_subs() * 4;
  }

  const auto oqf_fn = PathFilePath(config.datadir(), USER_QSC);
  File oqf(oqf_fn);
  if (!oqf.Open(File::modeBinary|File::modeReadWrite)) {
    messagebox(window, "Could not open user.qsc");
    return;
  }
  File nqf(PathFilePath(config.datadir(), "userqsc.new"));
  if (!nqf.Open(File::modeBinary|File::modeReadWrite|File::modeCreateFile|File::modeTruncate)) {
    messagebox(window, "Could not open userqsc.new");
    return;
  }

  const auto nu = oqf.length() / config.qscn_len();
  for (int i = 0; i < nu; i++) {
    if (i % 10 == 0) {
      window->Puts(StrCat(i, "/", nu, "\r"));
    }
    oqf.Read(oqsc, config.qscn_len());

    *nqsc = *oqsc;
    memcpy(nqsc_n, oqsc_n, l1);
    memcpy(nqsc_q, oqsc_q, l2);
    memcpy(nqsc_p, oqsc_p, l3);
    nqf.Write(nqsc, nqscn_len);
  }

  oqf.Close();
  nqf.Close();
  File::Remove(oqf_fn);
  File::Rename(nqf.path(), oqf.path());

  config.max_subs(num_subs);
  config.max_dirs(num_dirs);
  config.qscn_len(nqscn_len);
  window->Puts("Done\n");
}

void up_subs_dirs(wwiv::sdk::Config& config) {
  curses_out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(curses_out->CreateBoxedWindow("Update Sub/Directory Maximums", 16, 76));

  int y=1;
  window->PutsXY(2, y++, StrCat("Current max # subs: ", config.max_subs()));
  window->PutsXY(2, y++, StrCat("Current max # dirs: ", config.max_dirs()));

  if (dialog_yn(window.get(), "Change # subs or # dirs?")) { 
    y+=2;
    window->SetColor(SchemeId::INFO);
    window->PutsXY(2, y++, "Enter the new max subs/dirs you wish.  Just hit <enter> to leave that");
    window->PutsXY(2, y++, "value unchanged.  All values will be rounded up to the next 32.");
    window->PutsXY(2, y++, "Values can range from 32-1024");

    y++;
    window->SetColor(SchemeId::PROMPT);
    window->PutsXY(2, y++, "New max subs: ");
    auto num_subs = input_number<uint16_t>(window.get(), 4);
    if (!num_subs) {
      num_subs = config.max_subs();
    }
    window->SetColor(SchemeId::PROMPT);
    window->PutsXY(2, y++, "New max dirs: ");
    auto num_dirs = input_number<uint16_t>(window.get(), 4);
    if (!num_dirs) {
      num_dirs = config.max_dirs();
    }

    if (num_subs % 32) {
      num_subs = (num_subs / 32 + 1) * 32;
    }
    if (num_dirs % 32) {
      num_dirs = (num_dirs / 32 + 1) * 32;
    }

    if (num_subs < 32) {
      num_subs = 32;
    }
    if (num_dirs < 32) {
      num_dirs = 32;
    }

    if (num_subs > MAX_SUBS_DIRS) {
      num_subs = MAX_SUBS_DIRS;
    }
    if (num_dirs > MAX_SUBS_DIRS) {
      num_dirs = MAX_SUBS_DIRS;
    }

    if ((num_subs != config.max_subs()) || (num_dirs != config.max_dirs())) {
      const auto text = fmt::format("Change to {} subs and {} dirs? ", num_subs, num_dirs);
      if (dialog_yn(window.get(), text)) {
        window->SetColor(SchemeId::INFO);
        window->Puts("Please wait...\n");
        convert_to(window.get(), num_subs, num_dirs, config);
      }
    }
  }
}
