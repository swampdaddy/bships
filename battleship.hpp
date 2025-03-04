#pragma once

#include <sysio/sysio.hpp>
#include <string>
#include <vector>

using namespace sysio;
using std::string;
using std::vector;

// -----------------------------------------------------------------------------
// CONTRACT: battleship
// -----------------------------------------------------------------------------
CONTRACT battleship : public contract {
public:
   using contract::contract;

   // ------------------
   // ACTION declarations
   // ------------------
   ACTION create(const name& player1, const name& player2);
   ACTION commit(uint64_t game_id, const name& player, const checksum256& board_hash);
   ACTION startgame(uint64_t game_id);
   ACTION fire(uint64_t game_id, uint8_t row, uint8_t col);
   ACTION reveal(uint64_t game_id, const name& player, const string& board_str, const string& salt);

private:
   // Helper function to count ship cells (denoted by 'X') in a board string.
   int count_ship_cells(const std::string& board_str);

   // ------------------
   // TABLE: game
   // ------------------
   TABLE game {
      uint64_t     id;
      name         player1;
      name         player2;
      checksum256  player1_hash;      // Committed hash of player1's board
      checksum256  player2_hash;      // Committed hash of player2's board
      string       player1_revealed;  // Revealed board string for player1
      string       player2_revealed;  // Revealed board string for player2
      name         next_turn;         // Tracks whose turn it is
      name         winner;            // The winner, if any
      name         game_status;       // "created", "active", or "finished"

      uint64_t primary_key() const { return id; }
   };
   using game_index = multi_index<"games"_n, game>;

   // ------------------
   // TABLE: shot
   // ------------------
   TABLE shot {
      uint64_t id;
      name     firer;   // which player fired
      name     target;  // which player was targeted
      uint8_t  row;
      uint8_t  col;

      uint64_t primary_key() const { return id; }
   };
   using shot_index = multi_index<"shots"_n, shot>;
};
