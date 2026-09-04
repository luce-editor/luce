import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

const sidebars: SidebarsConfig = {
  docsSidebar: [
    {
      type: 'doc',
      id: 'intro',
      label: 'Wprowadzenie & Szybki Start',
    },
    {
      type: 'doc',
      id: 'architecture',
      label: 'Architektura Silnika',
    },
    {
      type: 'category',
      label: 'Motywy & Style',
      collapsible: true,
      collapsed: false,
      items: [
        'themes/overview',
        'themes/creating-themes',
      ],
    },
    {
      type: 'category',
      label: 'System Wtyczek',
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
      label: 'Interfejs i Układ',
      collapsible: true,
      collapsed: false,
      items: [
        'interface/overview',
        'interface/activity-bar',
        'interface/source-control',
        'interface/diagnostics',
        'interface/terminal',
        'interface/command-palette',
      ],
    },
    {
      type: 'category',
      label: 'Podświetlanie Składni',
      collapsible: true,
      collapsed: false,
      items: [
        'syntax/lexers',
      ],
    },
  ],
};

export default sidebars;
