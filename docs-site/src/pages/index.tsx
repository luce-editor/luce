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
            Blazing-Fast, Minimalist Modern C++23 Code Editor
          </Translate>
        </p>
        <div className={styles.buttons} style={{ display: 'flex', gap: '14px', justifyContent: 'center', flexWrap: 'wrap' }}>
          <Link
            className="button button--primary-luce button--lg"
            to="/docs/intro">
            <Translate id="homepage.getStarted">
              Get Started
            </Translate>
          </Link>
          <Link
            className="button button--secondary-luce button--lg"
            to="/docs/themes/overview">
            <Translate id="homepage.exploreThemes">
              Explore Themes
            </Translate>
          </Link>
          <Link
            className="button button--secondary-luce button--lg"
            to="/docs/plugins/architecture">
            <Translate id="homepage.pluginSdk">
              Plugin SDK
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
      title={`${siteConfig.title} — Documentation`}
      description="Official documentation and developer guides for the Luce C++23 code editor.">
      <HomepageHeader />
      <main>
        <HomepageFeatures />
      </main>
    </Layout>
  );
}
