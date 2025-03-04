#include "battleship.hpp"

// -----------------------------------------------------------------------------
// ACTION: create
// -----------------------------------------------------------------------------
ACTION battleship::create(const name& player1, const name& player2) {
   require_auth(player1);

   game_index games(get_self(), get_self().value);

   games.emplace(player1, [&](auto& row) {
      row.id          = games.available_primary_key();
      row.player1     = player1;
      row.player2     = player2;
      row.game_status = "created"_n;
      // next_turn stays empty until the game starts
   });
}

// -----------------------------------------------------------------------------
// ACTION: commit
// -----------------------------------------------------------------------------
ACTION battleship::commit(uint64_t game_id, const name& player, const checksum256& board_hash) {
   require_auth(player);

   game_index games(get_self(), get_self().value);
   auto itr = games.find(game_id);
   check(itr != games.end(), "Game not found.");

   check(player == itr->player1 || player == itr->player2, "Not a valid player for this game.");
   check(itr->game_status == "created"_n, "Cannot commit after game is active or finished.");

   games.modify(itr, same_payer, [&](auto& row) {
      if (player == row.player1) {
         check(row.player1_hash == checksum256(), "Player1 already committed.");
         row.player1_hash = board_hash;
      } else {
         check(row.player2_hash == checksum256(), "Player2 already committed.");
         row.player2_hash = board_hash;
      }
   });
}

// -----------------------------------------------------------------------------
// ACTION: startgame
// -----------------------------------------------------------------------------
ACTION battleship::startgame(uint64_t game_id) {
   game_index games(get_self(), get_self().value);
   auto itr = games.find(game_id);
   check(itr != games.end(), "Game not found.");

   // Require auth of one of the players
   require_auth(itr->player1);  
   // Or you could allow either player to start: 
   //   check(has_auth(itr->player1) || has_auth(itr->player2), "Not authorized.");

   check(itr->game_status == "created"_n, "Game not in 'created' state.");
   check(itr->player1_hash != checksum256(), "Player1 has not committed a board hash.");
   check(itr->player2_hash != checksum256(), "Player2 has not committed a board hash.");

   games.modify(itr, same_payer, [&](auto& row) {
      row.game_status = "active"_n;
      row.next_turn   = row.player1; // start with player1
   });
}

// -----------------------------------------------------------------------------
// ACTION: fire
// -----------------------------------------------------------------------------
ACTION battleship::fire(uint64_t game_id, uint8_t row, uint8_t col) {
   check(row < 10 && col < 10, "Coordinates must be within [0..9].");

   game_index games(get_self(), get_self().value);
   auto gitr = games.find(game_id);
   check(gitr != games.end(), "Game not found.");
   check(gitr->game_status == "active"_n, "Game is not active.");

   require_auth(gitr->next_turn);
   name current_player = gitr->next_turn;
   name opponent = (current_player == gitr->player1) ? gitr->player2 : gitr->player1;

   // Record the shot in the "shots" table (scoped by game_id)
   shot_index shots(get_self(), game_id);
   shots.emplace(current_player, [&](auto& s) {
      s.id      = shots.available_primary_key();
      s.firer   = current_player;
      s.target  = opponent;
      s.row     = row;
      s.col     = col;
   });

   // Switch turn to the other player
   games.modify(gitr, same_payer, [&](auto& row) {
      row.next_turn = opponent;
   });
}

// -----------------------------------------------------------------------------
// ACTION: reveal
// -----------------------------------------------------------------------------
ACTION battleship::reveal(uint64_t game_id,
                          const name& player,
                          const string& board_str,
                          const string& salt)
{
   require_auth(player);

   game_index games(get_self(), get_self().value);
   auto gitr = games.find(game_id);
   check(gitr != games.end(), "Game not found.");
   check(gitr->game_status == "active"_n, "Game must be active to reveal.");

   // Identify the stored commit hash
   checksum256 stored_hash = (player == gitr->player1) ? gitr->player1_hash : gitr->player2_hash;
   check(stored_hash != checksum256(), "No committed hash found for this player.");

   // Recompute the hash: (board_str + salt)
   std::string to_hash = board_str + salt;
   checksum256 computed_hash = sha256(to_hash.c_str(), to_hash.size());

   check(computed_hash == stored_hash, "Reveal does not match committed board hash!");

   // Store the revealed board if needed
   games.modify(gitr, same_payer, [&](auto& row) {
      if (player == row.player1) {
         row.player1_revealed = board_str;
      } else {
         row.player2_revealed = board_str;
      }
   });

   // Evaluate hits from opponent's shots
   shot_index shots(get_self(), game_id);
   auto shot_itr = shots.begin();

   int hits = 0;
   for (; shot_itr != shots.end(); ++shot_itr) {
      // Only check shots that targeted this revealing player
      if (shot_itr->target == player) {
         int cell_index = shot_itr->row * 10 + shot_itr->col;
         if (cell_index >= 0 && cell_index < (int)board_str.size()) {
            if (board_str[cell_index] == 'X') {
               hits++;
            }
         }
      }
   }

   // Check if all ships are sunk
   int total_ship_cells = count_ship_cells(board_str);
   if (hits >= total_ship_cells && total_ship_cells > 0) {
      // Opponent has destroyed all ships => Opponent is the winner
      name opponent = (player == gitr->player1) ? gitr->player2 : gitr->player1;
      games.modify(gitr, same_payer, [&](auto& row) {
         row.game_status = "finished"_n;
         row.winner      = opponent;
      });
   }
}

// -----------------------------------------------------------------------------
// HELPER: count_ship_cells
// -----------------------------------------------------------------------------
int battleship::count_ship_cells(const std::string& board_str) {
   int count = 0;
   for (char c : board_str) {
      if (c == 'X') {
         count++;
      }
   }
   return count;
}

// -----------------------------------------------------------------------------
// DISPATCH MACRO
// -----------------------------------------------------------------------------
SYSIO_DISPATCH(battleship,
   (create)
   (commit)
   (startgame)
   (fire)
   (reveal)
)
