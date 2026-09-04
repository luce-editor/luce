import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'Luce Editor',
  tagline: 'Niezwykle szybki, minimalistyczny edytor kodu w C++23',
  favicon: 'img/favicon.ico',

  url: 'https://luce-editor.github.io',
  baseUrl: '/luce/',

  organizationName: 'luce-editor',
  projectName: 'luce',

  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',

  i18n: {
    defaultLocale: 'pl',
    locales: ['pl', 'en'],
    localeConfigs: {
      pl: {
        label: 'Polski',
        direction: 'ltr',
        htmlLang: 'pl-PL',
      },
      en: {
        label: 'English',
        direction: 'ltr',
        htmlLang: 'en-US',
      },
    },
  },

  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
          routeBasePath: 'docs',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    image: 'img/luce-logo.png',
    colorMode: {
      defaultMode: 'dark',
      disableSwitch: false,
      respectPrefersColorScheme: false,
    },
    navbar: {
      title: 'Luce',
      logo: {
        alt: 'Luce Logo',
        src: 'img/luce-logo.png',
      },
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'docsSidebar',
          position: 'left',
          label: 'Dokumentacja',
        },
        {
          type: 'localeDropdown',
          position: 'right',
        },
        {
          href: 'https://github.com/luce-editor/luce',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Dokumentacja',
          items: [
            {
              label: 'Wprowadzenie',
              to: '/docs/intro',
            },
            {
              label: 'Motywy i Style',
              to: '/docs/themes/overview',
            },
            {
              label: 'System Wtyczek',
              to: '/docs/plugins/architecture',
            },
            {
              label: 'Interfejs i Układ',
              to: '/docs/interface/overview',
            },
          ],
        },
        {
          title: 'Architektura',
          items: [
            {
              label: 'Wirtualne Przewijanie',
              to: '/docs/architecture',
            },
            {
              label: 'Leksery i Składnia',
              to: '/docs/syntax/lexers',
            },
            {
              label: 'Referencja C ABI',
              to: '/docs/plugins/api-reference',
            },
          ],
        },
        {
          title: 'Projekt',
          items: [
            {
              label: 'Repozytorium GitHub',
              href: 'https://github.com/luce-editor/luce',
            },
            {
              label: 'Licencja (MIT)',
              to: '/docs/intro',
            },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} Luce Editor Project. Zbudowano przy użyciu Dear ImGui, SDL2, OpenGL i Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.vsDark,
      additionalLanguages: ['cpp', 'c', 'rust', 'cmake', 'css', 'json', 'bash', 'powershell', 'markdown'],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
