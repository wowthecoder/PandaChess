import { useState, useMemo } from 'react';
import './ChessBoard.css';

const PIECE_SYMBOLS = {
  k: { w: '\u2654', b: '\u265A' },
  q: { w: '\u2655', b: '\u265B' },
  r: { w: '\u2656', b: '\u265C' },
  b: { w: '\u2657', b: '\u265D' },
  n: { w: '\u2658', b: '\u265E' },
  p: { w: '\u2659', b: '\u265F' },
};

const FILES = 'abcdefgh';

export default function ChessBoard({
  game,
  playerColor,
  selectedSquare,
  legalMoves,
  lastMove,
  isThinking,
  isGameOver,
  onSquareClick,
  promotionPending,
  onPromotionSelect,
}) {
  const [hoveredSquare, setHoveredSquare] = useState(null);

  const isFlipped = playerColor === 'b';

  const ranks = useMemo(
    () => (isFlipped ? [1, 2, 3, 4, 5, 6, 7, 8] : [8, 7, 6, 5, 4, 3, 2, 1]),
    [isFlipped]
  );

  const files = useMemo(
    () => (isFlipped ? FILES.split('').reverse() : FILES.split('')),
    [isFlipped]
  );

  const legalTargetSet = useMemo(
    () => new Set(legalMoves.map((m) => m.to)),
    [legalMoves]
  );

  const captureTargetSet = useMemo(() => {
    const set = new Set();
    for (const m of legalMoves) {
      const target = game.get(m.to);
      if (target || m.flags.includes('e')) {
        set.add(m.to);
      }
    }
    return set;
  }, [legalMoves, game]);

  const turnColor = game.turn();
  const isInCheck = game.isCheck();

  const kingSquare = useMemo(() => {
    if (!isInCheck) return null;
    const board = game.board();
    for (let r = 0; r < 8; r++) {
      for (let c = 0; c < 8; c++) {
        const piece = board[r][c];
        if (piece && piece.type === 'k' && piece.color === turnColor) {
          return piece.square;
        }
      }
    }
    return null;
  }, [isInCheck, game, turnColor]);

  const isInteractive = !isThinking && !isGameOver;

  const getSquareClasses = (sq) => {
    const fi = sq.charCodeAt(0) - 97;
    const ri = parseInt(sq[1]) - 1;
    const isLight = (fi + ri) % 2 === 1;
    const piece = game.get(sq);
    const isOwnPiece = piece && piece.color === playerColor;
    const isLegalTarget = legalTargetSet.has(sq);

    let cls = `square ${isLight ? 'square-light' : 'square-dark'}`;
    if (selectedSquare === sq) cls += ' selected';
    if (lastMove && (lastMove.from === sq || lastMove.to === sq)) cls += ' last-move';
    if (kingSquare === sq) cls += ' in-check';
    if (isLegalTarget) cls += ' legal-target';
    if (isOwnPiece) cls += ' own-piece';
    if (isInteractive && (isOwnPiece || isLegalTarget)) cls += ' clickable';

    return cls;
  };

  const handleClick = (sq) => {
    if (!isInteractive) return;
    onSquareClick(sq);
  };

  return (
    <div className="board-container">
      <div className={`board-with-coords ${isThinking ? 'thinking' : ''}`}>
        {/* Board grid with inline rank labels */}
        <div className="board-inner">
          {ranks.map((rank, ri) => (
            <div key={rank} className="board-rank-row">
              <span className="coord-rank">{rank}</span>
              {files.map((file) => {
                const sq = `${file}${rank}`;
                const piece = game.get(sq);
                const isLegal = legalTargetSet.has(sq);
                const isCapture = captureTargetSet.has(sq);

                return (
                  <div
                    key={sq}
                    className={getSquareClasses(sq)}
                    onClick={() => handleClick(sq)}
                    onMouseEnter={() => setHoveredSquare(sq)}
                    onMouseLeave={() => setHoveredSquare(null)}
                  >
                    {piece && (
                      <span className={`piece piece-${piece.color}`}>
                        {PIECE_SYMBOLS[piece.type][piece.color]}
                      </span>
                    )}
                    {isLegal && !isCapture && !piece && <div className="legal-dot" />}
                    {isLegal && (isCapture || piece) && <div className="capture-ring" />}
                  </div>
                );
              })}
            </div>
          ))}

          {/* File labels */}
          <div className="board-file-row">
            <span className="coord-rank" />
            {files.map((f) => (
              <span key={f} className="coord-file">{f}</span>
            ))}
          </div>
        </div>

        {/* Promotion dialog */}
        {promotionPending && (
          <div className="promotion-overlay">
            <div className="promotion-dialog">
              {['q', 'r', 'b', 'n'].map((p) => (
                <div
                  key={p}
                  className="promotion-option"
                  onClick={() => onPromotionSelect(p)}
                >
                  <span className={`piece piece-${playerColor}`}>
                    {PIECE_SYMBOLS[p][playerColor]}
                  </span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
