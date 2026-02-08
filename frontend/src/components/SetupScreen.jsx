import { useState } from 'react';
import './SetupScreen.css';

const PLAY_STYLES = [
  { id: 'precise', name: 'Precise', desc: 'Safe play' },
  { id: 'balanced', name: 'Balanced', desc: 'Natural' },
  { id: 'creative', name: 'Creative', desc: 'Inventive' },
  { id: 'wild', name: 'Wild', desc: 'Chaotic' },
];

export default function SetupScreen({ onStart }) {
  const [playerColor, setPlayerColor] = useState('w');
  const [botType, setBotType] = useState('stockfish');
  const [searchDepth, setSearchDepth] = useState(12);
  const [numSimulations, setNumSimulations] = useState(1000);
  const [timeLimit, setTimeLimit] = useState(5);
  const [playStyle, setPlayStyle] = useState('balanced');

  const handleStart = () => {
    onStart({
      playerColor,
      botType,
      searchDepth: botType === 'stockfish' ? searchDepth : undefined,
      numSimulations: botType === 'leela' ? numSimulations : undefined,
      timeLimit,
      playStyle: botType === 'leela' ? playStyle : undefined,
    });
  };

  return (
    <div className="setup-overlay">
      <div className="setup-card">
        <div className="setup-header">
          <h1>PandaChess</h1>
          <p>Challenge the machine</p>
        </div>

        <div className="setup-divider" />

        <div className="setup-section">
          <span className="setup-label">Play as</span>
          <div className="color-select">
            <div
              className={`color-card white-card ${playerColor === 'w' ? 'selected' : ''}`}
              onClick={() => setPlayerColor('w')}
            >
              <span className="color-card-piece">&#9812;</span>
              <span className="color-card-label">White</span>
            </div>
            <div
              className={`color-card black-card ${playerColor === 'b' ? 'selected' : ''}`}
              onClick={() => setPlayerColor('b')}
            >
              <span className="color-card-piece">&#9818;</span>
              <span className="color-card-label">Black</span>
            </div>
          </div>
        </div>

        <div className="setup-divider" />

        <div className="setup-section">
          <span className="setup-label">Engine</span>
          <div className="engine-select">
            <div
              className={`engine-card ${botType === 'stockfish' ? 'selected' : ''}`}
              onClick={() => setBotType('stockfish')}
            >
              <div className="engine-card-title">Alpha-Beta + NNUE</div>
              <div className="engine-card-desc">Traditional search with neural evaluation</div>
            </div>
            <div
              className={`engine-card ${botType === 'leela' ? 'selected' : ''}`}
              onClick={() => setBotType('leela')}
            >
              <div className="engine-card-title">MCTS + Network</div>
              <div className="engine-card-desc">Monte Carlo tree search with policy network</div>
            </div>
          </div>
        </div>

        <div className="setup-divider" />

        <div className="setup-section">
          <span className="setup-label">Difficulty</span>

          {botType === 'stockfish' ? (
            <div className="param-group">
              <div className="param-header">
                <span className="param-name">Search Depth</span>
                <span className="param-value">{searchDepth}</span>
              </div>
              <input
                type="range"
                className="param-slider"
                min={1}
                max={20}
                value={searchDepth}
                onChange={(e) => setSearchDepth(Number(e.target.value))}
              />
            </div>
          ) : (
            <div className="param-group">
              <div className="param-header">
                <span className="param-name">Simulations</span>
                <span className="param-value">{numSimulations.toLocaleString()}</span>
              </div>
              <input
                type="range"
                className="param-slider"
                min={100}
                max={10000}
                step={100}
                value={numSimulations}
                onChange={(e) => setNumSimulations(Number(e.target.value))}
              />
            </div>
          )}

          <div className="param-group">
            <div className="param-header">
              <span className="param-name">Time Limit per Move</span>
              <span className="param-value">{timeLimit}s</span>
            </div>
            <input
              type="range"
              className="param-slider"
              min={1}
              max={30}
              value={timeLimit}
              onChange={(e) => setTimeLimit(Number(e.target.value))}
            />
          </div>
        </div>

        {botType === 'leela' && (
          <>
            <div className="setup-divider" />
            <div className="setup-section">
              <span className="setup-label">Play Style</span>
              <div className="style-select">
                {PLAY_STYLES.map((s) => (
                  <div
                    key={s.id}
                    className={`style-option ${playStyle === s.id ? 'selected' : ''}`}
                    onClick={() => setPlayStyle(s.id)}
                  >
                    <div className="style-option-name">{s.name}</div>
                    <div className="style-option-desc">{s.desc}</div>
                  </div>
                ))}
              </div>
            </div>
          </>
        )}

        <div className="setup-section">
          <button className="start-button" onClick={handleStart}>
            Begin Match
          </button>
        </div>
      </div>
    </div>
  );
}
