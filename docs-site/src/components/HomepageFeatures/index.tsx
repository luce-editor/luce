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
        Natywna Wydajność C++23
      </Translate>
    ),
    Icon: Zap,
    color: '#eab308',
    description: (
      <Translate id="feature.speed.desc">
        Start poniżej sekundy, minimalne zużycie pamięci i błyskawiczna responsywność dzięki Dear ImGui, SDL2 i OpenGL 3.3.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.themes.title">
        Silnik Motywów i Hot Reload
      </Translate>
    ),
    Icon: Palette,
    color: '#ec4899',
    description: (
      <Translate id="feature.themes.desc">
        Twórz własne motywy w czytelnym formacie CSS z pełną paletą tokenów interfejsu i składni, przeładowywane na żywo.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.plugins.title">
        Wtyczki w Języku Lua
      </Translate>
    ),
    Icon: Puzzle,
    color: '#10b981',
    description: (
      <Translate id="feature.plugins.desc">
        Pisz lekkie, bezpieczne rozszerzenia w języku Lua 5.4 z bezpośrednim dostępem do buforów, poleceń i paska stanu.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.layout.title">
        Układ Dokowania i Activity Bar
      </Translate>
    ),
    Icon: LayoutGrid,
    color: '#818cf8',
    description: (
      <Translate id="feature.layout.desc">
        Elegancki poziomy pasek aktywności oraz elastyczna przestrzeń dokowania okien zapamiętywana między sesjami.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.terminal.title">
        Wbudowany Terminal Podprocesu
      </Translate>
    ),
    Icon: Terminal,
    color: '#38bdf8',
    description: (
      <Translate id="feature.terminal.desc">
        Nieblokująca konsola terminala z dedykowanym polem wpisywania i automatycznym przewijaniem wprost w oknie edytora.
      </Translate>
    ),
  },
  {
    title: (
      <Translate id="feature.lexers.title">
        Wirtualne Przewijanie i Leksery
      </Translate>
    ),
    Icon: Code2,
    color: '#a855f7',
    description: (
      <Translate id="feature.lexers.desc">
        Płynna edycja plików 100k+ linii w stałych 60+ FPS dzięki inkrementalnemu buforowaniu tokenów i natywnym lekserom.
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
