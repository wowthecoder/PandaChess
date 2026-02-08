import './EvalBar.css';

export default function EvalBar({ score, isThinking }) {
  const s = typeof score === 'number' ? score : 0;

  // Sigmoid mapping: eval -> white percentage
  const sigmoid = 1 / (1 + Math.exp(-s * 0.4));
  const whitePct = sigmoid * 100;
  const blackPct = 100 - whitePct;

  // Format display score
  let displayScore;
  if (Math.abs(s) >= 100) {
    const mateIn = Math.abs(s) - 100;
    displayScore = `M${mateIn}`;
  } else if (s === 0) {
    displayScore = '0.0';
  } else {
    displayScore = (s >= 0 ? '+' : '') + s.toFixed(1);
  }

  // Determine label position
  const isEven = whitePct > 42 && whitePct < 58;

  return (
    <div className={`eval-bar ${isThinking ? 'pulsing' : ''}`}>
      <div className="eval-fill-black" style={{ flex: blackPct }} />
      <div className="eval-fill-white" style={{ flex: whitePct }} />

      {isEven ? (
        <span className="eval-label center">{displayScore}</span>
      ) : whitePct >= 58 ? (
        <span className="eval-label bottom">{displayScore}</span>
      ) : (
        <span className="eval-label top">{displayScore}</span>
      )}
    </div>
  );
}
