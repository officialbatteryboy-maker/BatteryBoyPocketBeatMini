import { useEffect, useMemo, useRef, useState } from "react";
import cityImage from "./assets/purple-city.jpg";

type Tab = "home" | "library" | "battle";
type StatName = "rhythm" | "resonance" | "discovery" | "harmony" | "energy" | "rarity";
type BattleMenu = "root" | "moves" | "stats";
type Song = {
  id: number;
  title: string;
  artist: string;
  album: string;
  year: number;
  genre: string;
  length: string;
  plays: number;
  colors: [string, string];
  affinity: "Pulse" | "Groove" | "Echo" | "Melody" | "Nocturne";
};

const songs: Song[] = [
  {
    id: 0,
    title: "Midnight City",
    artist: "M83",
    album: "Hurry Up, We're Dreaming",
    year: 2011,
    genre: "Electronic",
    length: "4:03",
    plays: 48,
    colors: ["#f25ca2", "#5d2fc2"],
    affinity: "Pulse",
  },
  {
    id: 1,
    title: "Cherry-coloured Funk",
    artist: "Cocteau Twins",
    album: "Heaven or Las Vegas",
    year: 1990,
    genre: "Dream Pop",
    length: "3:12",
    plays: 36,
    colors: ["#ff8c72", "#30318d"],
    affinity: "Nocturne",
  },
  {
    id: 2,
    title: "Digital Love",
    artist: "Daft Punk",
    album: "Discovery",
    year: 2001,
    genre: "French House",
    length: "4:58",
    plays: 29,
    colors: ["#4ed3ff", "#29205f"],
    affinity: "Groove",
  },
  {
    id: 3,
    title: "Resonance",
    artist: "HOME",
    album: "Odyssey",
    year: 2014,
    genre: "Synthwave",
    length: "3:32",
    plays: 21,
    colors: ["#f9be43", "#d23493"],
    affinity: "Pulse",
  },
  {
    id: 4,
    title: "A Walk",
    artist: "Tycho",
    album: "Dive",
    year: 2011,
    genre: "Ambient",
    length: "5:16",
    plays: 17,
    colors: ["#84e6d1", "#2469bb"],
    affinity: "Echo",
  },
];

const peers = [
  { name: "MOSSY-07", song: "Dreams", level: 18, strength: 82, affinity: "Melody" },
  { name: "NOVA-CYD", song: "Genesis", level: 14, strength: 67, affinity: "Nocturne" },
  { name: "BEATBOY", song: "Archie, Marry Me", level: 11, strength: 54, affinity: "Groove" },
];

const statLabels: Record<StatName, string> = {
  rhythm: "RHY",
  resonance: "RES",
  discovery: "DSC",
  harmony: "HAR",
  energy: "NRG",
  rarity: "RAR",
};

const moves = [
  { id: "bass", name: "BASS DROP", affinity: "Pulse", power: 44, accuracy: 95, pp: 15, stat: "energy" as StatName, effect: "May stagger" },
  { id: "chorus", name: "GOLDEN CHORUS", affinity: "Melody", power: 52, accuracy: 85, pp: 10, stat: "harmony" as StatName, effect: "High critical rate" },
  { id: "echo", name: "ECHO BLOOM", affinity: "Echo", power: 34, accuracy: 100, pp: 20, stat: "resonance" as StatName, effect: "Restores 8 HP" },
  { id: "riff", name: "NEON RIFF", affinity: "Groove", power: 40, accuracy: 100, pp: 15, stat: "rhythm" as StatName, effect: "Builds tempo" },
];

const baseStats = (song: Song, bonuses: Partial<Record<StatName, number>> = {}) => ({
  rhythm: 32 + (song.id * 7) % 18 + (bonuses.rhythm ?? 0),
  resonance: 38 + (song.plays % 17) + (bonuses.resonance ?? 0),
  discovery: 27 + (song.year % 19) + (bonuses.discovery ?? 0),
  harmony: 34 + (song.album.length % 15) + (bonuses.harmony ?? 0),
  energy: 36 + (song.title.length % 17) + (bonuses.energy ?? 0),
  rarity: 30 + (song.artist.length % 20) + (bonuses.rarity ?? 0),
});

function Icon({
  name,
  size = 16,
}: {
  name: "home" | "library" | "battle" | "bluetooth" | "play" | "pause" | "skip" | "back" | "search" | "disc";
  size?: number;
}) {
  const paths: Record<string, React.ReactNode> = {
    home: <><path d="m3 10 9-7 9 7" /><path d="M5 9v11h14V9M9 20v-6h6v6" /></>,
    library: <><path d="M5 4h14v16H5z" /><path d="M9 4v16M12.5 8h3M12.5 12h3M12.5 16h3" /></>,
    battle: <><path d="m6 4 12 16M18 4 6 20" /><path d="m4 3 4 1-3 3M20 3l-4 1 3 3M4 21l4-1-3-3M20 21l-4-1 3-3" /></>,
    bluetooth: <path d="m7 7 10 10-5 4V3l5 4L7 17" />,
    play: <path d="m9 6 9 6-9 6z" fill="currentColor" />,
    pause: <><path d="M9 6v12M15 6v12" strokeWidth="3" /></>,
    skip: <><path d="m7 7 7 5-7 5z" fill="currentColor" /><path d="M16 7v10" strokeWidth="2.5" /></>,
    back: <><path d="m17 7-7 5 7 5z" fill="currentColor" /><path d="M8 7v10" strokeWidth="2.5" /></>,
    search: <><circle cx="11" cy="11" r="6" /><path d="m16 16 4 4" /></>,
    disc: <><circle cx="12" cy="12" r="9" /><circle cx="12" cy="12" r="2" /><path d="M12 3a9 9 0 0 1 8.5 6" /></>,
  };
  return (
    <svg
      aria-hidden="true"
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      {paths[name]}
    </svg>
  );
}

function AlbumArt({ song, small = false }: { song: Song; small?: boolean }) {
  return (
    <div
      className={`album-art ${small ? "album-art--small" : ""}`}
      style={{ "--c1": song.colors[0], "--c2": song.colors[1] } as React.CSSProperties}
      aria-label={`${song.album} album art`}
    >
      <div className="album-sun" />
      <div className="album-grid" />
      <div className="album-star album-star--one">✦</div>
      <div className="album-star album-star--two">✦</div>
      <span className="album-monogram">{song.artist.slice(0, 1)}</span>
    </div>
  );
}

function LevelChip({ level }: { level: number }) {
  return <span className="level-chip">LV.{level}</span>;
}

export default function App() {
  const [tab, setTab] = useState<Tab>("home");
  const [currentId, setCurrentId] = useState(0);
  const [isPlaying, setIsPlaying] = useState(false);
  const [progress, setProgress] = useState(38);
  const [playCounts, setPlayCounts] = useState<Record<number, number>>(() => {
    try {
      return JSON.parse(localStorage.getItem("songdex-plays") || "{}");
    } catch {
      return {};
    }
  });
  const [levels, setLevels] = useState<Record<number, number>>(() => {
    try {
      return JSON.parse(localStorage.getItem("songdex-levels") || "{}");
    } catch {
      return {};
    }
  });
  const [completionStreaks, setCompletionStreaks] = useState<Record<number, number>>(() => {
    try {
      return JSON.parse(localStorage.getItem("songdex-streaks") || "{}");
    } catch {
      return {};
    }
  });
  const [statBonuses, setStatBonuses] = useState<Record<number, Partial<Record<StatName, number>>>>(() => {
    try {
      return JSON.parse(localStorage.getItem("songdex-stats") || "{}");
    } catch {
      return {};
    }
  });
  const [levelUpSong, setLevelUpSong] = useState<number | null>(null);
  const [surprise, setSurprise] = useState(false);
  const [toast, setToast] = useState("SD CARD • 24 TRACKS READY");
  const [bluetoothOpen, setBluetoothOpen] = useState(false);
  const [scanning, setScanning] = useState(false);
  const [speaker, setSpeaker] = useState("PocketBeat Mini");
  const [query, setQuery] = useState("");
  const [peerIndex, setPeerIndex] = useState<number | null>(null);
  const [playerHp, setPlayerHp] = useState(100);
  const [enemyHp, setEnemyHp] = useState(100);
  const [battleText, setBattleText] = useState("Pick a nearby trainer to begin.");
  const [battleMenu, setBattleMenu] = useState<BattleMenu>("root");
  const [battleBusy, setBattleBusy] = useState(false);
  const [turn, setTurn] = useState(1);
  const [focused, setFocused] = useState(false);
  const [selectedMove, setSelectedMove] = useState(0);
  const [movePp, setMovePp] = useState(moves.map((move) => move.pp));
  const completionHandledRef = useRef(false);
  const seekDisqualifiedRef = useRef(false);

  const current = songs[currentId];
  const songPlays = (song: Song) => playCounts[song.id] ?? song.plays;
  const songLevel = (song: Song) => levels[song.id] ?? Math.max(1, Math.floor(song.plays / 5) + 1);
  const songStats = (song: Song) => baseStats(song, statBonuses[song.id]);
  const strongest = useMemo(
    () => [...songs].sort((a, b) => songPlays(b) - songPlays(a))[0],
    [playCounts],
  );
  const filteredSongs = songs.filter((song) =>
    `${song.title} ${song.artist} ${song.album}`.toLowerCase().includes(query.toLowerCase()),
  );

  useEffect(() => {
    localStorage.setItem("songdex-plays", JSON.stringify(playCounts));
  }, [playCounts]);

  useEffect(() => {
    localStorage.setItem("songdex-levels", JSON.stringify(levels));
    localStorage.setItem("songdex-streaks", JSON.stringify(completionStreaks));
    localStorage.setItem("songdex-stats", JSON.stringify(statBonuses));
  }, [levels, completionStreaks, statBonuses]);

  useEffect(() => {
    if (!isPlaying) return;
    const interval = window.setInterval(() => {
      setProgress((value) => Math.min(100, value + 0.35));
    }, 500);
    return () => window.clearInterval(interval);
  }, [isPlaying]);

  useEffect(() => {
    if (progress < 100 || completionHandledRef.current) return;
    completionHandledRef.current = true;
    setIsPlaying(false);

    if (seekDisqualifiedRef.current) {
      setCompletionStreaks({});
      setToast("SEEKED PLAYBACK • LEVEL CHAIN RESET");
      return;
    }

    if (surprise) {
      setCompletionStreaks({});
      setToast("SURPRISE COMPLETE • STREAK NOT COUNTED");
      return;
    }

    const nextStreak = (completionStreaks[currentId] ?? 0) + 1;
    setPlayCounts((counts) => ({
      ...counts,
      [currentId]: (counts[currentId] ?? current.plays) + 1,
    }));
    if (nextStreak >= 5) {
      setCompletionStreaks({ [currentId]: 0 });
      setLevels((stored) => ({ ...stored, [currentId]: songLevel(current) + 1 }));
      setLevelUpSong(currentId);
      setToast("LEVEL UP • CHOOSE A STAT TO TRAIN");
    } else {
      setCompletionStreaks({ [currentId]: nextStreak });
      setToast(`FULL LISTEN ${nextStreak}/5 • KEEP THE CHAIN GOING`);
    }
  }, [progress, surprise, currentId]);

  const pickSong = (id: number, fromSurprise = false) => {
    if (id !== currentId || fromSurprise) setCompletionStreaks({});
    setCurrentId(id);
    setProgress(0);
    setIsPlaying(true);
    setSurprise(fromSurprise);
    completionHandledRef.current = false;
    seekDisqualifiedRef.current = false;
    setToast(fromSurprise ? "SURPRISE PLAY • NO XP OR STREAK" : "LISTEN TO THE END • CHAIN STARTED");
    setTab("home");
  };

  const togglePlay = () => {
    const next = !isPlaying;
    if (next && progress >= 100) {
      setProgress(0);
      completionHandledRef.current = false;
      seekDisqualifiedRef.current = false;
      setToast(`CHAIN ${completionStreaks[currentId] ?? 0}/5 • FULL PLAY REQUIRED`);
    }
    setIsPlaying(next);
  };

  const nextSong = (direction: number) => {
    const next = (currentId + direction + songs.length) % songs.length;
    pickSong(next);
  };

  const surpriseMe = () => {
    let id = Math.floor(Math.random() * songs.length);
    if (id === currentId) id = (id + 1) % songs.length;
    pickSong(id, true);
  };

  const scanBluetooth = () => {
    setScanning(true);
    window.setTimeout(() => setScanning(false), 1300);
  };

  const selectPeer = (index: number) => {
    setPeerIndex(index);
    setEnemyHp(100);
    setPlayerHp(100);
    setBattleMenu("root");
    setBattleBusy(false);
    setTurn(1);
    setFocused(false);
    setMovePp(moves.map((move) => move.pp));
    setBattleText(`${peers[index].name} sent out ${peers[index].song}!`);
  };

  const typeMultiplier = (attackType: string, targetType: string) => {
    const strongAgainst: Record<string, string> = {
      Pulse: "Nocturne",
      Nocturne: "Melody",
      Melody: "Groove",
      Groove: "Echo",
      Echo: "Pulse",
    };
    if (strongAgainst[attackType] === targetType) return 1.35;
    if (strongAgainst[targetType] === attackType) return 0.72;
    return 1;
  };

  const resolveEnemyTurn = (currentPlayerHp: number) => {
    if (peerIndex === null) return;
    window.setTimeout(() => {
      const enemyMove = ["STATIC WAVE", "DEEP CUT", "HOOK SHOT"][turn % 3];
      const damage = Math.round(10 + peers[peerIndex].strength / 11);
      const hp = Math.max(0, currentPlayerHp - damage);
      setPlayerHp(hp);
      setBattleText(
        hp === 0
          ? `${peers[peerIndex].song} used ${enemyMove}! Your track faded out...`
          : `${peers[peerIndex].song} used ${enemyMove}! ${damage} damage.`,
      );
      setBattleBusy(false);
      setTurn((value) => value + 1);
      setBattleMenu("root");
    }, 700);
  };

  const useMove = (index: number) => {
    if (peerIndex === null || battleBusy || movePp[index] <= 0) return;
    const move = moves[index];
    setSelectedMove(index);
    setBattleBusy(true);
    setMovePp((values) => values.map((value, moveIndex) => moveIndex === index ? value - 1 : value));

    if (Math.random() * 100 > move.accuracy) {
      setBattleText(`${strongest.title} used ${move.name}... but missed!`);
      resolveEnemyTurn(playerHp);
      return;
    }

    const multiplier = typeMultiplier(move.affinity, peers[peerIndex].affinity);
    const stat = songStats(strongest)[move.stat];
    const damage = Math.max(5, Math.round(move.power * (stat / 50) * 0.42 * multiplier * (focused ? 1.45 : 1)));
    const hp = Math.max(0, enemyHp - damage);
    setEnemyHp(hp);
    setFocused(false);
    if (move.id === "echo") setPlayerHp((value) => Math.min(100, value + 8));
    const effectiveness = multiplier > 1 ? " It's super effective!" : multiplier < 1 ? " It's not very effective." : "";
    setBattleText(`${strongest.title} used ${move.name}! ${damage} damage.${effectiveness}`);
    if (hp === 0) {
      setBattleText(`${strongest.title} wins! Victory data was added to its record.`);
      setBattleBusy(false);
      return;
    }
    resolveEnemyTurn(move.id === "echo" ? Math.min(100, playerHp + 8) : playerHp);
  };

  const focusTurn = () => {
    if (battleBusy) return;
    setBattleBusy(true);
    setFocused(true);
    setBattleText(`${strongest.title} listened closely. Its next move is amplified!`);
    resolveEnemyTurn(playerHp);
  };

  const chooseLevelStat = (stat: StatName) => {
    if (levelUpSong === null) return;
    setStatBonuses((stored) => ({
      ...stored,
      [levelUpSong]: {
        ...stored[levelUpSong],
        [stat]: (stored[levelUpSong]?.[stat] ?? 0) + 3,
      },
    }));
    setToast(`${stat.toUpperCase()} +3 • TRAINING SAVED`);
    setLevelUpSong(null);
  };

  return (
    <main className="app-shell">
      <div className="ambient ambient--one" />
      <div className="ambient ambient--two" />
      <section className="console" aria-label="SONGDEX music player">
        <div className="console-top">
          <span className="console-brand">SONGDEX</span>
          <span className="console-model">CYD • 001</span>
          <div className="speaker-slits"><i /><i /><i /><i /><i /></div>
        </div>

        <div className="screen-frame">
          <div className="screen" style={{ "--city": `url(${cityImage})` } as React.CSSProperties}>
            <div className="scanlines" />
            <header className="status-bar">
              <div className="status-brand"><span className="pulse-dot" /> SONGDEX</div>
              <div className="status-right">
                <button className="status-speaker" onClick={() => setBluetoothOpen(true)}>
                  <Icon name="bluetooth" size={12} /> {speaker}
                </button>
                <span>82%</span>
                <span className="battery"><i /></span>
              </div>
            </header>

            <div className="screen-content">
              {tab === "home" && (
                <div className="home-view view-enter">
                  <div className="hero-copy">
                    <span className="eyebrow">NOW SPINNING</span>
                    <h1>{current.title}</h1>
                    <p>{current.artist}</p>
                    <div className="hero-tags">
                      <LevelChip level={songLevel(current)} />
                      <span>{current.affinity}</span>
                      <span>{songPlays(current)} PLAYS</span>
                    </div>
                    <div className="chain-meter">
                      <span>LEVEL CHAIN</span>
                      <i>{[0, 1, 2, 3, 4].map((step) => <em className={step < (completionStreaks[currentId] ?? 0) ? "filled" : ""} key={step} />)}</i>
                      <b>{completionStreaks[currentId] ?? 0}/5</b>
                    </div>
                    <button className="surprise-button" onClick={surpriseMe}>
                      <span className="surprise-spark">✦</span>
                      <span><b>SURPRISE ME!</b><small>LET FATE PICK A TRACK</small></span>
                      <span className="button-arrow">›</span>
                    </button>
                  </div>

                  <div className="art-stage">
                    <div className={`art-disc ${isPlaying ? "is-spinning" : ""}`} />
                    <AlbumArt song={current} />
                    <span className="rare-label">★ RARE PRESSING</span>
                  </div>

                  <div className="player-card">
                    <button className="round-control small" onClick={() => nextSong(-1)} aria-label="Previous track">
                      <Icon name="back" size={15} />
                    </button>
                    <button className="round-control play" onClick={togglePlay} aria-label={isPlaying ? "Pause" : "Play"}>
                      <Icon name={isPlaying ? "pause" : "play"} size={18} />
                    </button>
                    <button className="round-control small" onClick={() => nextSong(1)} aria-label="Next track">
                      <Icon name="skip" size={15} />
                    </button>
                    <div className="progress-wrap">
                      <input
                        aria-label="Track progress"
                        type="range"
                        min="0"
                        max="100"
                        value={progress}
                        onChange={(event) => {
                          const nextProgress = Number(event.target.value);
                          if (nextProgress > progress + 1) {
                            seekDisqualifiedRef.current = true;
                            setToast("FAST-FORWARD ALLOWED • THIS PLAY WILL NOT COUNT");
                          }
                          setProgress(nextProgress);
                        }}
                        style={{ "--progress": `${progress}%` } as React.CSSProperties}
                      />
                      <div className="time-row"><span>1:32</span><span>{current.length}</span></div>
                    </div>
                    <button className="info-chip" onClick={() => setTab("library")}>INFO</button>
                  </div>
                  <div className="toast-line">{toast}</div>
                </div>
              )}

              {tab === "library" && (
                <div className="library-view view-enter">
                  <div className="view-heading">
                    <div><span className="eyebrow">SD COLLECTION</span><h2>Your Songdex</h2></div>
                    <div className="collection-count"><b>24</b><span>TRACKS</span></div>
                  </div>
                  <label className="search-box">
                    <Icon name="search" size={14} />
                    <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="Search your collection..." />
                    <span>SELECT</span>
                  </label>
                  <div className="song-list">
                    {filteredSongs.map((song, index) => (
                      <button className={`song-row ${song.id === currentId ? "is-current" : ""}`} key={song.id} onClick={() => pickSong(song.id)}>
                        <span className="song-number">{String(index + 1).padStart(2, "0")}</span>
                        <AlbumArt song={song} small />
                        <span className="song-copy">
                          <b>{song.title}</b>
                          <small>{song.artist} • {song.album}</small>
                        </span>
                        <span className="song-stats"><LevelChip level={songLevel(song)} /><small>{song.affinity} • {songPlays(song)} PLAYS</small></span>
                        <span className="song-chevron">›</span>
                      </button>
                    ))}
                  </div>
                  <div className="metadata-strip">
                    <Icon name="disc" size={17} />
                    <span><b>{current.album}</b><small>{current.year} • {current.genre} • MP3 320 KBPS</small></span>
                    <span className="metadata-length">{current.length}</span>
                  </div>
                </div>
              )}

              {tab === "battle" && (
                <div className="battle-view view-enter">
                  <div className="view-heading battle-heading">
                    <div><span className="eyebrow">WIRELESS ARENA</span><h2>Song Battle</h2></div>
                    <span className="online-pill"><i /> ONLINE</span>
                  </div>

                  {peerIndex === null ? (
                    <>
                      <div className="champion-card">
                        <div className="champion-art"><AlbumArt song={strongest} small /></div>
                        <div><span className="eyebrow">YOUR CHAMPION</span><h3>{strongest.title}</h3><p>{strongest.artist}</p></div>
                        <div className="power-score"><span>POWER</span><b>{songStats(strongest).energy + songLevel(strongest)}</b><LevelChip level={songLevel(strongest)} /></div>
                      </div>
                      <div className="nearby-title"><span>TRAINERS NEARBY</span><span className="radar">⌁ SCANNING</span></div>
                      <div className="peer-grid">
                        {peers.map((peer, index) => (
                          <button className="peer-card" key={peer.name} onClick={() => selectPeer(index)}>
                            <span className={`peer-avatar peer-avatar--${index + 1}`}>{peer.name.slice(0, 1)}</span>
                            <span className="peer-copy"><b>{peer.name}</b><small>{peer.song} • LV.{peer.level}</small></span>
                            <span className="signal"><i /><i /><i /></span>
                            <span className="challenge">CHALLENGE</span>
                          </button>
                        ))}
                      </div>
                      <p className="battle-note">Battles use local Wi-Fi • no internet required</p>
                    </>
                  ) : (
                    <div className="arena">
                      <div className="arena-sky">
                        <div className="turn-indicator">TURN {turn} <span>{battleBusy ? "ENEMY PHASE" : "YOUR PHASE"}</span></div>
                        <div className="combatant enemy">
                          <div className="fighter-card enemy-card">{peers[peerIndex].song.slice(0, 1)}</div>
                          <div className="hp-panel"><b>{peers[peerIndex].song}</b><span>LV.{peers[peerIndex].level}</span><i><em className={enemyHp < 30 ? "hp-low" : ""} style={{ width: `${enemyHp}%` }} /></i><small>{peers[peerIndex].affinity} • {enemyHp}/100</small></div>
                        </div>
                        <div className="combatant player">
                          <div className="hp-panel"><b>{strongest.title}</b><span>LV.{songLevel(strongest)}</span><i><em className={playerHp < 30 ? "hp-low" : ""} style={{ width: `${playerHp}%` }} /></i><small>{strongest.affinity} • {playerHp}/100 {focused ? "• FOCUSED" : ""}</small></div>
                          <AlbumArt song={strongest} small />
                        </div>
                      </div>
                      <div className="battle-console">
                        <div className="battle-dialogue"><p>{battleText}</p><small>{battleBusy ? "WAIT..." : "WHAT WILL YOUR TRACK DO?"}</small></div>
                        {battleMenu === "root" && (
                          <div className="battle-actions">
                            <button onClick={() => setBattleMenu("moves")} disabled={battleBusy || enemyHp === 0 || playerHp === 0}><b>FIGHT</b><small>CHOOSE A MOVE</small></button>
                            <button onClick={() => setBattleMenu("stats")}><b>STATS</b><small>VIEW BUILD</small></button>
                            <button onClick={focusTurn} disabled={battleBusy || enemyHp === 0 || playerHp === 0}><b>FOCUS</b><small>BOOST NEXT HIT</small></button>
                            {(enemyHp === 0 || playerHp === 0) ? (
                              <button onClick={() => { setEnemyHp(100); setPlayerHp(100); setTurn(1); setFocused(false); setMovePp(moves.map((move) => move.pp)); setBattleText("The rematch begins! Choose your opening."); }}><b>REMATCH</b><small>RESTORE BOTH</small></button>
                            ) : (
                              <button onClick={() => setPeerIndex(null)}><b>RUN</b><small>FIND TRAINER</small></button>
                            )}
                          </div>
                        )}
                        {battleMenu === "moves" && (
                          <div className="move-menu">
                            <div className="move-grid">
                              {moves.map((move, index) => (
                                <button
                                  className={selectedMove === index ? "selected" : ""}
                                  onMouseEnter={() => setSelectedMove(index)}
                                  onFocus={() => setSelectedMove(index)}
                                  onClick={() => useMove(index)}
                                  disabled={battleBusy || movePp[index] === 0}
                                  key={move.id}
                                >
                                  <span className={`move-type type-${move.affinity.toLowerCase()}`}>{move.affinity}</span>
                                  <b>{move.name}</b>
                                  <small>POW {move.power} • ACC {move.accuracy}</small>
                                  <em>PP {movePp[index]}/{move.pp}</em>
                                </button>
                              ))}
                            </div>
                            <div className="move-detail"><span>{moves[selectedMove].effect}</span><button onClick={() => setBattleMenu("root")}>BACK</button></div>
                          </div>
                        )}
                        {battleMenu === "stats" && (
                          <div className="battle-stat-sheet">
                            <div className="stat-sheet-head"><span>{strongest.affinity} TYPE</span><button onClick={() => setBattleMenu("root")}>BACK</button></div>
                            <div className="stat-grid">
                              {(Object.keys(statLabels) as StatName[]).map((stat) => (
                                <div key={stat}><span>{statLabels[stat]}</span><b>{songStats(strongest)[stat]}</b><i><em style={{ width: `${Math.min(100, songStats(strongest)[stat])}%` }} /></i></div>
                              ))}
                            </div>
                          </div>
                        )}
                      </div>
                    </div>
                  )}
                </div>
              )}
            </div>

            <nav className="bottom-nav">
              <button className={tab === "home" ? "active" : ""} onClick={() => setTab("home")}><Icon name="home" size={15} /><span>PLAYER</span></button>
              <button className={tab === "library" ? "active" : ""} onClick={() => setTab("library")}><Icon name="library" size={15} /><span>SONGDEX</span></button>
              <button className={tab === "battle" ? "active" : ""} onClick={() => setTab("battle")}><Icon name="battle" size={15} /><span>BATTLE</span><i className="nav-badge">3</i></button>
            </nav>

            {bluetoothOpen && (
              <div className="modal-shade" onClick={() => setBluetoothOpen(false)}>
                <div className="bluetooth-modal" onClick={(e) => e.stopPropagation()}>
                  <div className="modal-title"><span className="bluetooth-icon"><Icon name="bluetooth" size={18} /></span><div><span className="eyebrow">AUDIO OUTPUT</span><h3>Bluetooth Link</h3></div><button onClick={() => setBluetoothOpen(false)}>×</button></div>
                  <button className="scan-button" onClick={scanBluetooth}><span className={scanning ? "is-scanning" : ""}><Icon name="search" size={14} /></span>{scanning ? "SCANNING AIRWAVES..." : "SCAN FOR SPEAKERS"}</button>
                  <div className="device-list">
                    {["PocketBeat Mini", "JBL Flip 6", "Studio Buds"].map((device, index) => (
                      <button key={device} className={speaker === device ? "connected" : ""} onClick={() => { setSpeaker(device); setToast(`CONNECTED • ${device.toUpperCase()}`); }}>
                        <span className="device-icon">♪</span><span><b>{device}</b><small>{speaker === device ? "CONNECTED • SBC 44.1KHZ" : index === 1 ? "AVAILABLE • -52 DB" : "PAIRED"}</small></span><i>{speaker === device ? "✓" : "›"}</i>
                      </button>
                    ))}
                  </div>
                  <p>Playback continues while scanning.</p>
                </div>
              </div>
            )}

            {levelUpSong !== null && (
              <div className="modal-shade level-up-shade">
                <div className="level-up-modal">
                  <div className="level-burst">LEVEL<br /><b>{songLevel(songs[levelUpSong])}</b></div>
                  <div className="level-up-copy"><span className="eyebrow">FIVE-PLAY CHAIN COMPLETE</span><h3>{songs[levelUpSong].title} grew!</h3><p>Choose one attribute to train. This permanent +3 bonus also affects battle moves.</p></div>
                  <div className="level-stat-choices">
                    {(Object.keys(statLabels) as StatName[]).map((stat) => (
                      <button onClick={() => chooseLevelStat(stat)} key={stat}>
                        <span>{statLabels[stat]}</span><b>{songStats(songs[levelUpSong])[stat]}</b><em>+3</em>
                      </button>
                    ))}
                  </div>
                  <small>Music playback remains available after training.</small>
                </div>
              </div>
            )}
          </div>
        </div>

        <div className="console-bottom">
          <div className="console-note"><span>POWER</span><i /></div>
          <div className="console-controls">
            <span className="select-pill">SELECT</span><span className="select-pill">START</span>
            <span className="button-b">B</span><span className="button-a">A</span>
          </div>
        </div>
      </section>
      <p className="desktop-caption">A pocket archive for the music that made you.</p>
    </main>
  );
}
