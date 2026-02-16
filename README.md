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