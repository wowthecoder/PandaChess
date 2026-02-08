import { useState, useRef, useCallback, useEffect } from 'react';
import { Chess } from 'chess.js';
import SetupScreen from './components/SetupScreen';
import ChessBoard from './components/ChessBoard';
import EvalBar from './components/EvalBar';
import InfoPanel from './components/InfoPanel';
import * as api from './api';
import './App.css';

const MOCK_MODE = !import.meta.env.VITE_API_BASE;

function generateMockPV(game) {
  const moves = game.moves();
  if (moves.length === 0) return '';
  const count = Math.min(moves.length, 4 + Math.floor(Math.random() * 4));
  const pvMoves = [];
  const tempGame = new Chess(game.fen());
  for (let i = 0; i < count; i++) {
    const available = tempGame.moves();
    if (available.length === 0) break;
    const m = available[Math.floor(Math.random() * available.length)];
    tempGame.move(m);
    pvMoves.push(m);
  }
  return pvMoves.join(' ');
}

export default function App() {
  const [screen, setScreen] = useState('setup');
  const [settings, setSettings] = useState(null);
  const [gameId, setGameId] = useState(null);

  const gameRef = useRef(new Chess());
  const [position, setPosition] = useState('start');
  const [playerColor, setPlayerColor] = useState('w');

  const [selectedSquare, setSelectedSquare] = useState(null);
  const [legalMoves, setLegalMoves] = useState([]);
  const [lastMove, setLastMove] = useState(null);
  const [promotionPending, setPromotionPending] = useState(null);

  const [isThinking, setIsThinking] = useState(false);
  const [thinkingElapsed, setThinkingElapsed] = useState(0);
  const [evalScore, setEvalScore] = useState(0);
  const [pvLine, setPvLine] = useState('');
  const [thinkingTime, setThinkingTime] = useState(0);
  const [moveHistory, setMoveHistory] = useState([]);
  const [gameResult, setGameResult] = useState(null);

  const thinkingTimerRef = useRef(null);

  const game = gameRef.current;
  const isPlayerTurn = game.turn() === playerColor;

  // Live thinking timer
  useEffect(() => {
    if (isThinking) {
      const start = Date.now();
      setThinkingElapsed(0);
      thinkingTimerRef.current = setInterval(() => {
        setThinkingElapsed(((Date.now() - start) / 1000).toFixed(1));
      }, 100);
    } else {
      clearInterval(thinkingTimerRef.current);
    }
    return () => clearInterval(thinkingTimerRef.current);
  }, [isThinking]);

  const syncState = useCallback(() => {
    const g = gameRef.current;
    setPosition(g.fen());
    setMoveHistory(g.history({ verbose: true }));
    setSelectedSquare(null);
    setLegalMoves([]);

    if (g.isCheckmate()) {
      const winner = g.turn() === 'w' ? 'b' : 'w';
      setGameResult({
        winner,
        message:
          winner === playerColor ? 'Checkmate — You win!' : 'Checkmate — You lost.',
      });
    } else if (g.isStalemate()) {
      setGameResult({ winner: 'draw', message: 'Stalemate — Draw.' });
    } else if (g.isDraw()) {
      setGameResult({ winner: 'draw', message: 'Draw.' });
    }
  }, [playerColor]);

  const requestBotMove = useCallback(
    async (g) => {
      setIsThinking(true);
      const currentGame = g || gameRef.current;

      if (MOCK_MODE) {
        await new Promise((r) =>
          setTimeout(r, 600 + Math.random() * 1400)
        );
        const moves = currentGame.moves({ verbose: true });
        if (moves.length === 0) {
          setIsThinking(false);
          syncState();
          return;
        }
        const botMove = moves[Math.floor(Math.random() * moves.length)];
        currentGame.move(botMove);
        setLastMove({ from: botMove.from, to: botMove.to });
        setEvalScore(parseFloat((Math.random() * 2 - 1).toFixed(2)));
        setPvLine(generateMockPV(currentGame));
        setThinkingTime(thinkingElapsed || '1.0');
      } else {
        try {
          const history = currentGame.history({ verbose: true });
          const lastPlayerMove = history.length > 0 ? history[history.length - 1] : null;
          const response = await api.sendPlayerMove(
            gameId,
            lastPlayerMove
              ? { from: lastPlayerMove.from, to: lastPlayerMove.to, promotion: lastPlayerMove.promotion }
              : null,
            currentGame.fen()
          );
          if (response.botMove) {
            const move = currentGame.move(response.botMove);
            if (move) {
              setLastMove({ from: move.from, to: move.to });
            }
          }
          setEvalScore(response.eval ?? 0);
          setPvLine(response.pv ?? '');
          setThinkingTime(response.thinkingTime ?? 0);
        } catch (e) {
          console.error('Bot move failed:', e);
        }
      }

      setIsThinking(false);
      syncState();
    },
    [gameId, syncState, thinkingElapsed]
  );

  const handleStartGame = useCallback(
    async (config) => {
      setSettings(config);
      setPlayerColor(config.playerColor);

      const newGame = new Chess();
      gameRef.current = newGame;

      setSelectedSquare(null);
      setLegalMoves([]);
      setLastMove(null);
      setEvalScore(0);
      setPvLine('');
      setThinkingTime(0);
      setMoveHistory([]);
      setGameResult(null);
      setPromotionPending(null);
      setPosition(newGame.fen());

      if (!MOCK_MODE) {
        try {
          const result = await api.startGame(config);
          setGameId(result.gameId);
          if (result.botMove) {
            newGame.move(result.botMove);
            setLastMove({ from: result.botMove.from, to: result.botMove.to });
            syncState();
          }
        } catch (e) {
          console.error('Failed to start game:', e);
        }
      } else {
        setGameId(`mock-${Date.now()}`);
      }

      setScreen('game');

      if (config.playerColor === 'b') {
        setTimeout(() => requestBotMove(newGame), 300);
      }
    },
    [requestBotMove, syncState]
  );

  const executeMove = useCallback(
    (from, to, promotion) => {
      const g = gameRef.current;
      const moveObj = { from, to };
      if (promotion) moveObj.promotion = promotion;

      const result = g.move(moveObj);
      if (!result) return false;

      setLastMove({ from, to });
      setPromotionPending(null);
      syncState();

      if (!g.isGameOver()) {
        setTimeout(() => requestBotMove(g), 200);
      }
      return true;
    },
    [syncState, requestBotMove]
  );

  const handleSquareClick = useCallback(
    (sq) => {
      const g = gameRef.current;
      if (g.turn() !== playerColor) return;
      if (gameResult) return;

      const piece = g.get(sq);

      // If a piece is selected and clicked square is a legal move
      if (selectedSquare) {
        const isLegalTarget = legalMoves.some((m) => m.to === sq);

        if (isLegalTarget) {
          // Check for promotion
          const selectedPiece = g.get(selectedSquare);
          const isPromotion =
            selectedPiece?.type === 'p' &&
            ((selectedPiece.color === 'w' && sq[1] === '8') ||
              (selectedPiece.color === 'b' && sq[1] === '1'));

          if (isPromotion) {
            setPromotionPending({ from: selectedSquare, to: sq });
            return;
          }

          executeMove(selectedSquare, sq);
          return;
        }

        // Clicked own piece -> switch selection
        if (piece && piece.color === playerColor) {
          const moves = g.moves({ square: sq, verbose: true });
          setSelectedSquare(sq);
          setLegalMoves(moves);
          return;
        }

        // Clicked elsewhere -> deselect
        setSelectedSquare(null);
        setLegalMoves([]);
        return;
      }

      // No selection: select own piece
      if (piece && piece.color === playerColor) {
        const moves = g.moves({ square: sq, verbose: true });
        setSelectedSquare(sq);
        setLegalMoves(moves);
      }
    },
    [playerColor, selectedSquare, legalMoves, gameResult, executeMove]
  );

  const handlePromotionSelect = useCallback(
    (piece) => {
      if (!promotionPending) return;
      executeMove(promotionPending.from, promotionPending.to, piece);
    },
    [promotionPending, executeMove]
  );

  const handleUndo = useCallback(() => {
    const g = gameRef.current;
    // Undo bot move + player move
    g.undo();
    g.undo();
    setLastMove(null);
    setEvalScore(0);
    setPvLine('');
    setThinkingTime(0);
    syncState();
  }, [syncState]);

  const handleResign = useCallback(() => {
    const winner = playerColor === 'w' ? 'b' : 'w';
    setGameResult({
      winner,
      message: 'You resigned.',
    });
    if (!MOCK_MODE && gameId) {
      api.resignGame(gameId).catch(console.error);
    }
  }, [playerColor, gameId]);

  const handleNewGame = useCallback(() => {
    setScreen('setup');
    setGameResult(null);
  }, []);

  const canUndo =
    !gameResult &&
    isPlayerTurn &&
    gameRef.current.history().length >= 2;

  if (screen === 'setup') {
    return (
      <div className="app">
        <SetupScreen onStart={handleStartGame} />
      </div>
    );
  }

  return (
    <div className="app">
      <header className="app-header">
        <div className="app-logo">
          <span className="app-logo-icon">&#9823;</span>
          <h1>PandaChess</h1>
          <span className="app-logo-tag">v0.1</span>
        </div>
      </header>

      <div className="game-container">
        <div className="board-area" style={{ position: 'relative' }}>
          <EvalBar
            score={evalScore}
            isThinking={isThinking}
          />
          <ChessBoard
            game={gameRef.current}
            playerColor={playerColor}
            selectedSquare={selectedSquare}
            legalMoves={legalMoves}
            lastMove={lastMove}
            isThinking={isThinking}
            isGameOver={!!gameResult}
            onSquareClick={handleSquareClick}
            promotionPending={promotionPending}
            onPromotionSelect={handlePromotionSelect}
          />
        </div>

        <InfoPanel
          settings={settings}
          evalScore={evalScore}
          pvLine={pvLine}
          thinkingTime={isThinking ? thinkingElapsed : thinkingTime}
          isThinking={isThinking}
          moveHistory={moveHistory}
          gameResult={gameResult}
          onNewGame={handleNewGame}
          onUndo={handleUndo}
          onResign={handleResign}
          canUndo={canUndo}
        />
      </div>
    </div>
  );
}
