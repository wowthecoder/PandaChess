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