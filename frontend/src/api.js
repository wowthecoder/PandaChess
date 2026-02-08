const API_BASE = import.meta.env.VITE_API_BASE || '/api';

export async function startGame(settings) {
  const response = await fetch(`${API_BASE}/game/start`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(settings),
  });
  if (!response.ok) throw new Error('Failed to start game');
  return response.json();
}

export async function sendPlayerMove(gameId, move, fen) {
  const response = await fetch(`${API_BASE}/game/move`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ gameId, move, fen }),
  });
  if (!response.ok) throw new Error('Failed to send move');
  return response.json();
}

export async function requestUndo(gameId) {
  const response = await fetch(`${API_BASE}/game/undo`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ gameId }),
  });
  if (!response.ok) throw new Error('Failed to undo');
  return response.json();
}

export async function resignGame(gameId) {
  const response = await fetch(`${API_BASE}/game/resign`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ gameId }),
  });
  if (!response.ok) throw new Error('Failed to resign');
  return response.json();
}
