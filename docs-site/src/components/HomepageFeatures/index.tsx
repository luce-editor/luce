import type {ReactNode} from 'react';
import clsx from 'clsx';
import Heading from '@theme/Heading';
import Translate from '@docusaurus/Translate';
import {
  Zap,
  Palette,
  Puzzle,
  LayoutGrid,
  Terminal,
  Code2,
  type LucideIcon,
} from 'lucide-react';

type FeatureItem = {
  title: ReactNode;
  Icon: LucideIcon;
  color: string;
  description: ReactNode;
};

const FeatureList: FeatureItem[] = [
  {
    title: (
      <Translate id="feature.speed.title">
        Native C++23 Performance
      </Translate>
    ),
    Icon: Zap,
    color: '#eab308',
    description: (
      <Translate id="feature.speed.desc">
        Sub-second cold startup, minimal memory footprint, and instant responsiveness powered by Dear ImGui, SDL2, and OpenGL 3.3.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.themes.title">
        Theme Engine & Hot Reload
      </Translate>
    ),
    Icon: Palette,
    color: '#ec4899',
    description: (
      <Translate id="feature.themes.desc">
        Create custom themes in intuitive CSS syntax with full UI and syntax token control, reloaded instantly on save.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.plugins.title">
        Lua Plugin Ecosystem
      </Translate>
    ),
    Icon: Puzzle,
    color: '#10b981',
    description: (
      <Translate id="feature.plugins.desc">
        Write lightweight, sandboxed extensions in Lua 5.4 with direct access to buffers, editor commands, and the status bar.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.layout.title">
        Docking Layout & Activity Bar
      </Translate>
    ),
    Icon: LayoutGrid,
    color: '#818cf8',
    description: (
      <Translate id="feature.layout.desc">
        Clean horizontal activity bar with a flexible multi-panel docking workspace persisted seamlessly across sessions.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.terminal.title">
        Embedded Subprocess Terminal
      </Translate>
    ),
    Icon: Terminal,
    color: '#38bdf8',
    description: (
      <Translate id="feature.terminal.desc">
        Non-blocking VT100 console tabs with dedicated input bars, process lifecycle management, and auto-scroll built directly into the editor.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.lexers.title">
        Virtual Scrolling & Fast Lexers
      </Translate>
    ),
    Icon: Code2,
    color: '#a855f7',
    description: (
      <Translate id="feature.lexers.desc">
        Butter-smooth 60+ FPS navigation even in 100k+ line documents powered by per-line token caches and deterministic lexers.
      </Translate>
    ),
  },
];

function Feature({title, Icon, color, description}: FeatureItem) {
  return (
    <div className={clsx('col col--4')} style={{ marginBottom: '24px' }}>
      <div className="luce-card">
        <div style={{ color, marginBottom: '14px', display: 'flex', alignItems: 'center' }}>
          <Icon size={28} strokeWidth={2} />
        </div>
        <Heading as="h3" className="luce-card-title">
          {title}
        </Heading>
        <p className="luce-card-desc">
          {description}
        </p>
      </div>
    </div>
  );
}

export default function HomepageFeatures(): ReactNode {
  return (
    <section className="luce-features-section">
      <div className="container">
        <div className="row">
          {FeatureList.map((props, idx) => (
            <Feature key={idx} {...props} />
          ))}
        </div>
      </div>
    </section>
  );
}
