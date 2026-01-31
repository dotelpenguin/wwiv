/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
/**************************************************************************/
#include "sdk/instance.h"
#include "wwivutil/instance/instance.h"

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/stl.h"
#include "core/strings.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace std::chrono;

namespace wwiv::wwivutil {

/* Instance status flags */
constexpr int INST_FLAGS_NONE = 0x0000;  // No flags at all
constexpr int INST_FLAGS_ONLINE = 0x0001;  // User online
constexpr int INST_FLAGS_MSG_AVAIL = 0x0002;  // Available for inst messages
constexpr int INST_FLAGS_INVIS = 0x0004;  // For invisibility


class InstanceDumpCommand final : public UtilCommand {
public:
  InstanceDumpCommand(): UtilCommand("dump", "Displays WWIV instance information.") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  dump : Displays instance information." << std::endl << std::endl;
    return ss.str();
  }

  [[nodiscard]] static std::string flags_to_string(uint16_t flags) {
    std::ostringstream ss;
    if (flags & INST_FLAGS_ONLINE) {
      ss << "[online] ";
    }
    if (flags & INST_FLAGS_MSG_AVAIL) {
      ss << "[msg avail] ";
    }
    if (flags & INST_FLAGS_INVIS) {
      ss << "[invisible] ";
    }
    return ss.str();
  }

  int Execute() override {
    Instances instances(*config()->config());
    if (!instances) {
      std::cout << "Unable to read Instance information.";
      return 1;
    }
    const auto num = instances.size();
    std::cout << "num instances:  " << num << std::endl;
    for (const auto& instance : instances) {
      std::cout << "=======================================================================" << std::endl;
      std::cout << "Instance    : #" << instance.node_number() << std::endl;
      std::cout << "User        : #" << instance.user_number() << std::endl;
      std::cout << "Location    : " << instance.location_description() << std::endl;
      std::cout << "SubLoc      : " << instance.subloc_code() << std::endl;
      std::cout << "Flags       : " << flags_to_string(instance.ir().flags) << std::endl;
      std::cout << "Modem Speed : " << instance.modem_speed() << std::endl;
      std::cout << "Started     : " << instance.started().to_string() << std::endl;
      std::cout << "Updated     : " << instance.updated().to_string() << std::endl;
    }
    std::cout << "=======================================================================" << std::endl;
    return 0;
  }

  bool AddSubCommands() override {
    return true;
  }

};

class InstanceCheckStaleCommand final : public UtilCommand {
public:
  InstanceCheckStaleCommand(): UtilCommand("checkstale", "Checks for stale instance.dat entries.") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  checkstale [--hours=N] : Checks for stale instances (default: 6 hours)" << std::endl << std::endl;
    return ss.str();
  }

  bool AddSubCommands() override {
    add_argument({"hours", "Number of hours before an instance is considered stale (default: 6)", "6"});
    return true;
  }

  int Execute() override {
    Instances instances(*config()->config());
    if (!instances) {
      std::cout << "Unable to read Instance information." << std::endl;
      return 1;
    }

    const auto stale_threshold_hours = iarg("hours");
    const auto stale_threshold = hours(stale_threshold_hours);
    const auto now = system_clock::now();
    auto stale_count = 0;

    std::cout << "Checking for stale instances (threshold: " << stale_threshold_hours << " hours)..." << std::endl;
    std::cout << std::endl;

    for (const auto& instance : instances) {
      // Skip instance 0 as it will likely always be stale
      if (instance.node_number() == 0) {
        continue;
      }

      const auto updated_time = instance.updated().to_system_clock();
      const auto time_since_update = now - updated_time;

      if (time_since_update > stale_threshold) {
        stale_count++;
        const auto stale_duration = duration_cast<duration<double>>(time_since_update);
        std::cout << "STALE: Node #" << instance.node_number() << std::endl;
        std::cout << "  User        : #" << instance.user_number() << std::endl;
        std::cout << "  Location    : " << instance.location_description() << std::endl;
        std::cout << "  Last Update : " << instance.updated().to_string() << std::endl;
        std::cout << "  Stale For   : " << wwiv::core::to_string(stale_duration) << std::endl;
        std::cout << std::endl;
      }
    }

    if (stale_count == 0) {
      std::cout << "No stale instances found." << std::endl;
      return 0;
    }

    std::cout << "Found " << stale_count << " stale instance(s)." << std::endl;
    return stale_count;
  }
};

// Helper function to find stale instance node numbers
static std::vector<int> find_stale_instances(Instances& instances, int stale_threshold_hours) {
  std::vector<int> stale_list;
  const auto stale_threshold = hours(stale_threshold_hours);
  const auto now = system_clock::now();

  for (const auto& instance : instances) {
    // Skip instance 0 as it will likely always be stale
    if (instance.node_number() == 0) {
      continue;
    }

    const auto updated_time = instance.updated().to_system_clock();
    const auto time_since_update = now - updated_time;

    if (time_since_update > stale_threshold) {
      stale_list.push_back(instance.node_number());
    }
  }

  return stale_list;
}

class InstanceFixStaleCommand final : public UtilCommand {
public:
  InstanceFixStaleCommand(): UtilCommand("fixstale", "Fixes stale instance.dat entries by resetting them to 'Waiting For Call'.") {}

  [[nodiscard]] std::string GetUsage() const override {
    std::ostringstream ss;
    ss << "Usage: " << std::endl << std::endl;
    ss << "  fixstale [--hours=N] [--force] : Fixes stale instances (default: 6 hours)" << std::endl;
    ss << "    --force : Skip confirmation prompt" << std::endl << std::endl;
    return ss.str();
  }

  bool AddSubCommands() override {
    add_argument({"hours", "Number of hours before an instance is considered stale (default: 6)", "6"});
    add_argument(BooleanCommandLineArgument("force", "Skip confirmation prompt", false));
    return true;
  }

  int Execute() override {
    Instances instances(*config()->config());
    if (!instances) {
      std::cout << "Unable to read Instance information." << std::endl;
      return 1;
    }

    const auto stale_threshold_hours = iarg("hours");
    const auto force = barg("force");

    std::cout << "Checking for stale instances (threshold: " << stale_threshold_hours << " hours)..." << std::endl;
    std::cout << std::endl;

    auto stale_list = find_stale_instances(instances, stale_threshold_hours);

    if (stale_list.empty()) {
      std::cout << "No stale instances found." << std::endl;
      return 0;
    }

    // Display stale instances
    const auto now = system_clock::now();
    for (const auto node_num : stale_list) {
      const auto instance = instances.at(node_num);
      const auto updated_time = instance.updated().to_system_clock();
      const auto time_since_update = now - updated_time;
      const auto stale_duration = duration_cast<duration<double>>(time_since_update);
      
      std::cout << "STALE: Node #" << node_num << std::endl;
      std::cout << "  User        : #" << instance.user_number() << std::endl;
      std::cout << "  Location    : " << instance.location_description() << std::endl;
      std::cout << "  Last Update : " << instance.updated().to_string() << std::endl;
      std::cout << "  Stale For   : " << wwiv::core::to_string(stale_duration) << std::endl;
      std::cout << std::endl;
    }

    std::cout << "Found " << stale_list.size() << " stale instance(s)." << std::endl;

    // Prompt for confirmation unless --force
    if (!force) {
      std::cout << std::endl;
      std::cout << "Reset these instances to 'Waiting For Call'? (y/n): ";
      std::string response;
      std::cin >> response;
      if (response != "y" && response != "Y" && response != "yes" && response != "YES") {
        std::cout << "Cancelled." << std::endl;
        return 0;
      }
    }

    // Fix stale instances
    auto fixed_count = 0;
    for (const auto node_num : stale_list) {
      // Get the instance record and modify it
      auto instance = instances.at(node_num);
      auto ir = instance.ir();
      
      // Reset to "Waiting For Call" state
      ir.number = static_cast<int16_t>(node_num);  // Ensure number matches
      ir.loc = INST_LOC_WFC;
      ir.subloc = 0;
      ir.flags = INST_FLAGS_NONE;  // Clear ONLINE and other flags
      ir.user = 0;  // Clear user number
      ir.modem_speed = 0;  // Clear modem speed
      
      // Update the instance
      if (instances.upsert(node_num, ir)) {
        // Verify the write by reading it back
        const auto verify_instance = instances.at(node_num);
        if (verify_instance.loc_code() == INST_LOC_WFC && 
            verify_instance.user_number() == 0 &&
            !verify_instance.online()) {
          std::cout << "Fixed Node #" << node_num << " (reset to 'Waiting For Call')" << std::endl;
          fixed_count++;
        } else {
          std::cerr << "WARNING: Node #" << node_num << " was written but verification failed." << std::endl;
          std::cerr << "  Location: " << verify_instance.location_description() << std::endl;
          std::cerr << "  User: #" << verify_instance.user_number() << std::endl;
          std::cerr << "  Online: " << (verify_instance.online() ? "yes" : "no") << std::endl;
        }
      } else {
        std::cerr << "ERROR: Failed to update Node #" << node_num << std::endl;
      }
    }

    std::cout << std::endl;
    std::cout << "Fixed " << fixed_count << " stale instance(s)." << std::endl;
    return stale_list.size() - fixed_count;  // Return number of failures
  }
};

bool InstanceCommand::AddSubCommands() {
  add(std::make_unique<InstanceDumpCommand>());
  add(std::make_unique<InstanceCheckStaleCommand>());
  add(std::make_unique<InstanceFixStaleCommand>());
  return true;
}


}  // namespace
