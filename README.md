# PandaChess
Experiments for chess engine 

# Engine Testing 
```
cd engine
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

For testing in `cutechess-cli`, do 
```
cd engine
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

To estimate ELO Rating, run this in the terminal (in the engine folder):
```
cutechess-cli \
  -engine cmd=./build/panda-chess name=Panda proto=uci \
  -engine cmd=/usr/games/stockfish name=Stockfish proto=uci option.UCI_LimitStrength=true option.UCI_Elo=1600 \
  -each tc=5+0.1 \
  -openings file=silversuite.pgn format=pgn order=random \
  -games 200 -repeat -recover
```

For 1600 ELO Stockfish, this is the output for commit id `f61dde94ba3af4a78ec63956733fbbcc1521be71`
```
Score of Panda vs Stockfish: 84 - 90 - 26  [0.485] 200
...      Panda playing White: 39 - 44 - 17  [0.475] 100
...      Panda playing Black: 45 - 46 - 9  [0.495] 100
...      White vs Black: 85 - 89 - 26  [0.490] 200
Elo difference: -10.4 +/- 45.1, LOS: 32.5 %, DrawRatio: 13.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Panda
   "Draw by 3-fold repetition": 24
   "Draw by fifty moves rule": 1
   "Draw by insufficient mating material": 1
   "Loss: Black mates": 44
   "Loss: White mates": 46
   "Win: Black mates": 45
   "Win: White mates": 39
Player: Stockfish
   "Draw by 3-fold repetition": 24
   "Draw by fifty moves rule": 1
   "Draw by insufficient mating material": 1
   "Loss: Black mates": 45
   "Loss: White mates": 39
   "Win: Black mates": 44
   "Win: White mates": 46
Finished match
```


Against 2000 elo Stockfish with tapered eval and better position eval:
```
Score of Panda vs Stockfish: 25 - 153 - 22  [0.180] 200
...      Panda playing White: 15 - 75 - 10  [0.200] 100
...      Panda playing Black: 10 - 78 - 12  [0.160] 100
...      White vs Black: 93 - 85 - 22  [0.520] 200
Elo difference: -263.4 +/- 57.6, LOS: 0.0 %, DrawRatio: 11.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Panda
   "Draw by 3-fold repetition": 17
   "Draw by fifty moves rule": 2
   "Draw by insufficient mating material": 3
   "Loss: Black mates": 75
   "Loss: White mates": 78
   "Win: Black mates": 10
   "Win: White mates": 15
Player: Stockfish
   "Draw by 3-fold repetition": 17
   "Draw by fifty moves rule": 2
   "Draw by insufficient mating material": 3
   "Loss: Black mates": 10
   "Loss: White mates": 15
   "Win: Black mates": 75
   "Win: White mates": 78
Finished match
```

With Principle Variation Search:
```
Finished game 200 (Stockfish vs Panda): 1-0 {White mates}
Score of Panda vs Stockfish: 30 - 145 - 25  [0.212] 200
...      Panda playing White: 17 - 68 - 15  [0.245] 100
...      Panda playing Black: 13 - 77 - 10  [0.180] 100
...      White vs Black: 94 - 81 - 25  [0.532] 200
Elo difference: -227.6 +/- 53.8, LOS: 0.0 %, DrawRatio: 12.5 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: Panda
   "Draw by 3-fold repetition": 20
   "Draw by fifty moves rule": 5
   "Loss: Black mates": 68
   "Loss: White mates": 77
   "Win: Black mates": 13
   "Win: White mates": 17
Player: Stockfish
   "Draw by 3-fold repetition": 20
   "Draw by fifty moves rule": 5
   "Loss: Black mates": 13
   "Loss: White mates": 17
   "Win: Black mates": 68
   "Win: White mates": 77
Finished match
```

Right now I am playing my engine against stock fish, and I noticed the following differences:
1. Nodes per second: My NPS is consistently higher than Stockfish and increasing as the game goes on (from 400k to 1100k+ vs 400k~ for stockfish)
2. Hash table usage: Also increasing as the game goes on, first 35 moves is lower than stockfish (stockfish is consistenly getting 80-90%, while my engine started from 30% and grew to 90% beyond move 35)
3. Search depth: My engine mostly stops at depth 9, only in the late game it started going up to 11 and 12. In contrast stockfish manages to get to depth 22-30 (some numbers are reported in slashes idk why, like 22/30, 32/16). Did i just not report my search depth in UCI correctly?
4.  I am playing tournament format 40 moves in 5 mins, and my engine divides the time evenly (7.5s per move). Stockfish took 22s on the very first move and most of the subsequent moves are much faster (2-5s), with a few moves taking longer. How is this adaptive time search implemented?