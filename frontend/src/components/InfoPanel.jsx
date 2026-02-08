import { useRef, useEffect } from 'react';
import './InfoPanel.css';

export default function InfoPanel({
  settings,
  evalScore,
  pvLine,
  thinkingTime,
  isThinking,
  moveHistory,
  gameResult,
  onNewGame,
  onUndo,
  onResign,
  canUndo,
}) {
  const moveListRef = useRef(null);

  useEffect(() => {
    if (moveListRef.current) {
      moveListRef.current.scrollTop = moveListRef.current.scrollHeight;
    }
  }, [moveHistory]);

  const formatEval = (score) => {
    if (score == null) return '—';
    if (Math.abs(score) >= 100) {
      const mateIn = Math.abs(score) - 100;
      return `${score > 0 ? '+' : '-'}M${mateIn}`;
    }
    return (score >= 0 ? '+' : '') + score.toFixed(1);
  };

  const evalClass =
    evalScore > 0.3 ? 'eval-positive' : evalScore < -0.3 ? 'eval-negative' : '';

  // Group moves into pairs (1. e4 e5, 2. Nf3 Nc6, ...)
  const movePairs = [];
  for (let i = 0; i < moveHistory.length; i += 2) {
    movePairs.push({
      number: Math.floor(i / 2) + 1,
      white: moveHistory[i],
      black: moveHistory[i + 1] || null,
    });
  }

  const isLatestMove = (idx) => idx === moveHistory.length - 1;

  return (
    <div className="info-panel">
      <div className="bot-header">
        <div className="bot-header-title">
          {settings?.botType === 'stockfish' ? 'Alpha-Beta + NNUE' : 'MCTS + Network'}
        </div>
        <div className="bot-header-subtitle">
          {settings?.botType === 'stockfish'
            ? `Depth ${settings.searchDepth} · ${settings.timeLimit}s/move`
            : `${settings?.numSimulations?.toLocaleString()} sims · ${settings?.timeLimit}s · ${settings?.playStyle || 'balanced'}`}
        </div>
      </div>

      <div className="analysis-section">
        <div className="analysis-row">
          <span className="analysis-label">Eval</span>
          <span className={`analysis-value ${evalClass}`}>{formatEval(evalScore)}</span>
        </div>
        <div className="analysis-row">
          <span className="analysis-label">PV</span>
          <span className="analysis-value pv" title={pvLine || ''}>
            {pvLine || '—'}
          </span>
        </div>
        <div className="analysis-row">
          <span className="analysis-label">Time</span>
          <span className="analysis-value">{thinkingTime ? `${thinkingTime}s` : '—'}</span>
        </div>

        {isThinking && (
          <div className="thinking-badge">
            <div className="thinking-dots">
              <span />
              <span />
              <span />
            </div>
            <span className="thinking-badge-text">Thinking</span>
          </div>
        )}
      </div>

      <div className="move-history">
        <div className="move-history-header">Moves</div>
        <div className="move-list" ref={moveListRef}>
          {movePairs.length === 0 && (
            <div style={{ color: 'var(--cream-faint)', fontSize: '1.2rem', fontStyle: 'italic', padding: '0.5rem 0' }}>
              No moves yet
            </div>
          )}
          {movePairs.map((pair) => (
            <div key={pair.number} className="move-pair">
              <span className="move-number">{pair.number}.</span>
              <span
                className={`move-san ${isLatestMove((pair.number - 1) * 2) ? 'latest' : ''}`}
              >
                {pair.white?.san || ''}
              </span>
              {pair.black && (
                <span
                  className={`move-san ${isLatestMove((pair.number - 1) * 2 + 1) ? 'latest' : ''}`}
                >
                  {pair.black.san}
                </span>
              )}
            </div>
          ))}
        </div>
      </div>

      {gameResult && (
        <div className="game-result">
          <div
            className={`result-text ${
              gameResult.winner === 'draw'
                ? 'draw'
                : gameResult.winner === settings?.playerColor
                  ? 'win'
                  : 'loss'
            }`}
          >
            {gameResult.message}
          </div>
        </div>
      )}

      <div className="game-controls">
        <button className="ctrl-btn primary" onClick={onNewGame}>
          New Game
        </button>
        <button className="ctrl-btn" onClick={onUndo} disabled={!canUndo || isThinking}>
          Undo
        </button>
        <button
          className="ctrl-btn danger"
          onClick={onResign}
          disabled={!!gameResult || isThinking}
        >
          Resign
        </button>
      </div>
    </div>
  );
}
