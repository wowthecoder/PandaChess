# Engine Testing 
```
cd engine
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

To build for testing in `cutechess-cli`, do 
```
cd engine
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The `frontend` folder contains a React website for a human to play chess against a bot.

The `engine` folder contains a C++ chess engine that utilizes negamax algorithm similar to Stockfish. There is a Readme file in the `engine` folder that gives a summary of what is in the engine.

Always test after you modify the chess engine files.