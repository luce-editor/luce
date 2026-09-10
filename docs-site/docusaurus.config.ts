import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'Luce Editor',
  tagline: 'Blazing-Fast, Minimalist Modern C++23 Code Editor',
  favicon: 'img/favicon.ico',

  url: 'https://luce-editor.github.io',
  baseUrl: '/luce/',

  organizationName: 'luce-editor',
  projectName: 'luce',

  onBrokenLinks: 'throw',
  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en', 'pl'],
    localeConfigs: {
      en: {
        label: 'English',
        direction: 'ltr',
        htmlLang: 'en-US',
      },
      pl: {
        label: 'Polski',
        direction: 'ltr',
        htmlLang: 'pl-PL',
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
          label: 'Documentation',
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
          title: 'Documentation',
          items: [
            {
              label: 'Introduction',
              to: '/docs/intro',
            },
            {
              label: 'Themes & Styling',
              to: '/docs/themes/overview',
            },
            {
              label: 'Plugin System',
              to: '/docs/plugins/architecture',
            },
            {
              label: 'Interface & Layout',
              to: '/docs/interface/overview',
            },
          ],
        },
        {
          title: 'Architecture',
          items: [
            {
              label: 'Virtual Scrolling',
              to: '/docs/architecture',
            },
            {
              label: 'Lexers & Syntax',
              to: '/docs/syntax/lexers',
            },
            {
              label: 'Lua Plugin API',
              to: '/docs/plugins/api-reference',
            },
          ],
        },
        {
          title: 'Project',
          items: [
            {
              label: 'GitHub Repository',
              href: 'https://github.com/luce-editor/luce',
            },
            {
              label: 'License (MIT)',
              to: '/docs/intro',
            },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} Luce Editor Project. Built with Dear ImGui, SDL2, OpenGL, and Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.vsDark,
      additionalLanguages: ['cpp', 'c', 'rust', 'cmake', 'css', 'json', 'bash', 'powershell', 'markdown', 'lua'],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
