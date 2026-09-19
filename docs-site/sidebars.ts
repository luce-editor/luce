import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

const sidebars: SidebarsConfig = {
  docsSidebar: [
    {
      type: 'doc',
      id: 'intro',
      label: 'Introduction & Quick Start',
    },
    {
      type: 'doc',
      id: 'installation',
      label: 'Installation & Setup',
    },
    {
      type: 'doc',
      id: 'architecture',
      label: 'Engine Architecture',
    },
    {
      type: 'category',
      label: 'Themes & Styling',
      collapsible: true,
      collapsed: false,
      items: [
        'themes/overview',
        'themes/creating-themes',
      ],
    },
    {
      type: 'category',
      label: 'Plugin System',
      collapsible: true,
      collapsed: false,
      items: [
        'plugins/architecture',
        'plugins/api-reference',
        'plugins/creating-a-plugin',
      ],
    },
    {
      type: 'category',
      label: 'Interface & Layout',
      collapsible: true,
      collapsed: false,
      items: [
        'interface/overview',
        'interface/welcome',
        'interface/settings',
        'interface/activity-bar',
        'interface/source-control',
        'interface/diagnostics',
        'interface/terminal',
        'interface/command-palette',
        'interface/keyboard-shortcuts',
      ],
    },
    {
      type: 'category',
      label: 'Syntax Highlighting',
      collapsible: true,
      collapsed: false,
      items: [
        'syntax/lexers',
      ],
    },
  ],
};

export default sidebars;
