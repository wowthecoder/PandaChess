import { useMemo } from 'react';
import { Chessboard } from 'react-chessboard';
import './ChessBoard.css';

const BOARD_PIXEL_SIZE = 72 * 8;

const PROMOTION_LABELS = {
  q: 'Queen',
  r: 'Rook',
  b: 'Bishop',
  n: 'Knight',
};

function isLightSquare(square) {
  const fileIndex = square.charCodeAt(0) - 97;
  const rankIndex = parseInt(square[1], 10) - 1;
  return (fileIndex + rankIndex) % 2 === 1;
}

function squareBaseColor(square, selectedSquare, lastMove) {
  const light = isLightSquare(square);
  if (selectedSquare === square) {
    return light ? 'var(--board-light-selected)' : 'var(--board-dark-selected)';
  }
  if (lastMove && (lastMove.from === square || lastMove.to === square)) {
    return light ? 'var(--board-light-lastmove)' : 'var(--board-dark-lastmove)';
  }
  return light ? 'var(--board-light)' : 'var(--board-dark)';
}

function squareNaturalColor(square) {
  return isLightSquare(square) ? 'var(--board-light)' : 'var(--board-dark)';
}

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
  const isFlipped = playerColor === 'b';

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
  const position = game.fen();

  const customSquareStyles = useMemo(() => {
    const styles = {};
    const trackedSquares = new Set(legalTargetSet);
    if (selectedSquare) trackedSquares.add(selectedSquare);
    if (kingSquare) trackedSquares.add(kingSquare);
    if (lastMove) {
      trackedSquares.add(lastMove.from);
      trackedSquares.add(lastMove.to);
    }

    for (const square of trackedSquares) {
      const layers = [];
      const baseColor =
        kingSquare === square
          ? squareNaturalColor(square)
          : squareBaseColor(square, selectedSquare, lastMove);

      if (kingSquare === square) {
        layers.push(
          'radial-gradient(circle at center, rgba(255, 30, 30, 0.65) 0%, rgba(220, 40, 40, 0.3) 40%, transparent 70%)'
        );
      }

      if (legalTargetSet.has(square)) {
        if (captureTargetSet.has(square)) {
          layers.push(
            'radial-gradient(circle at center, transparent 54%, rgba(0, 0, 0, 0.22) 56%, rgba(0, 0, 0, 0.22) 68%, transparent 70%)'
          );
        } else {
          layers.push(
            'radial-gradient(circle at center, rgba(0, 0, 0, 0.22) 0%, rgba(0, 0, 0, 0.22) 16%, transparent 18%)'
          );
        }
      }

      styles[square] = {
        background: layers.length > 0 ? `${layers.join(', ')}, ${baseColor}` : baseColor,
      };
    }

    return styles;
  }, [captureTargetSet, kingSquare, lastMove, legalTargetSet, selectedSquare]);

  const handleSquareClick = (square) => {
    if (!isInteractive) return;
    onSquareClick(square);
  };

  return (
    <div className="board-container">
      <div className={`board-shell ${isThinking ? 'thinking' : ''}`}>
        <div className="board-react-root">
          <Chessboard
            id="panda-chessboard"
            position={position}
            boardWidth={BOARD_PIXEL_SIZE}
            boardOrientation={isFlipped ? 'black' : 'white'}
            arePiecesDraggable={false}
            onSquareClick={handleSquareClick}
            showBoardNotation
            customSquareStyles={customSquareStyles}
            customLightSquareStyle={{ backgroundColor: 'var(--board-light)' }}
            customDarkSquareStyle={{ backgroundColor: 'var(--board-dark)' }}
            customBoardStyle={{ boxShadow: 'none' }}
          />
        </div>

        {promotionPending && (
          <div className="promotion-overlay">
            <div className="promotion-dialog">
              {['q', 'r', 'b', 'n'].map((piece) => (
                <button
                  type="button"
                  key={piece}
                  className="promotion-option"
                  onClick={() => onPromotionSelect(piece)}
                >
                  <span className="promotion-piece-code">{piece.toUpperCase()}</span>
                  <span className="promotion-piece-label">{PROMOTION_LABELS[piece]}</span>
                </button>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
