#pragma once

#include <string>

#include "domain/common/players.hpp"

namespace snake {

/**
 * @brief Game identifier type
 */
using GameId = std::string;

/**
 * @brief Request to quit the application
 */
struct QuitMsg {
  // Empty - just a signal to initiate shutdown
};

}  // namespace snake
