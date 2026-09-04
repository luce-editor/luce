import type {ReactNode} from 'react';
import clsx from 'clsx';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import HomepageFeatures from '@site/src/components/HomepageFeatures';
import Heading from '@theme/Heading';
import Translate from '@docusaurus/Translate';
import useBaseUrl from '@docusaurus/useBaseUrl';

import styles from './index.module.css';

function HomepageHeader() {
  const {siteConfig} = useDocusaurusContext();
  return (
    <header className={clsx('hero hero--primary', styles.heroBanner)}>
      <div className="container">
        <img 
          src={useBaseUrl('/img/luce-logo.png')} 
          alt="Luce Logo" 
          style={{ width: '105px', height: '105px', marginBottom: '16px' }} 
        />
        <Heading as="h1" className="hero__title">
          {siteConfig.title}
        </Heading>
        <p className="hero__subtitle">
          <Translate id="homepage.tagline">
            Niezwykle szybki, minimalistyczny edytor kodu w C++23
          </Translate>
        </p>
        <div className={styles.buttons} style={{ display: 'flex', gap: '14px', justifyContent: 'center', flexWrap: 'wrap' }}>
          <Link
            className="button button--primary-luce button--lg"
            to="/docs/intro">
            <Translate id="homepage.getStarted">
              Rozpocznij
            </Translate>
          </Link>
          <Link
            className="button button--secondary-luce button--lg"
            to="/docs/themes/overview">
            <Translate id="homepage.exploreThemes">
              Przeglądaj Motywy
            </Translate>
          </Link>
          <Link
            className="button button--secondary-luce button--lg"
            to="/docs/plugins/architecture">
            <Translate id="homepage.pluginSdk">
              SDK Wtyczek
            </Translate>
          </Link>
        </div>
      </div>
    </header>
  );
}

export default function Home(): ReactNode {
  const {siteConfig} = useDocusaurusContext();
  return (
    <Layout
      title={`${siteConfig.title} — Dokumentacja`}
      description="Oficjalna dokumentacja i poradnik dewelopera dla edytora kodu Luce C++23.">
      <HomepageHeader />
      <main>
        <HomepageFeatures />
      </main>
    </Layout>
  );
}
