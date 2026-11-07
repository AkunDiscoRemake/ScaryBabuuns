// Monta a versão web em web/: junta o código (esbuild) e copia os assets.
// uso: node tools/build_web.mjs [--serve]
import { build, context } from 'esbuild';
import { cpSync, mkdirSync, copyFileSync, existsSync, readdirSync, writeFileSync } from 'fs';
import { execSync } from 'child_process';

const OUT = 'web';
mkdirSync(`${OUT}/assets/tex`, { recursive: true });
cpSync('app/assets/models', `${OUT}/assets/models`, { recursive: true });
cpSync('app/assets/snd', `${OUT}/assets/snd`, { recursive: true });
copyFileSync('app/assets/map.txt', `${OUT}/assets/map.txt`);
copyFileSync('src/index.html', `${OUT}/index.html`);
writeFileSync(`${OUT}/.nojekyll`, '');
// texturas .tex -> .png
if (!existsSync(`${OUT}/assets/tex/concrete.png`) || process.argv.includes('--tex'))
  execSync(`python3 tools/tex_to_png.py app/assets/tex ${OUT}/assets/tex`, { stdio: 'inherit' });

const opts = {
  entryPoints: ['src/main.js'], bundle: true, minify: true, sourcemap: false,
  target: ['chrome66'], outfile: `${OUT}/bundle.js`, logLevel: 'info',
};
if (process.argv.includes('--serve')) {
  const ctx = await context(opts);
  await ctx.watch();
  const { port } = await ctx.serve({ servedir: OUT, port: 8080 });
  console.log(`http://localhost:${port}`);
} else {
  await build(opts);
  console.log('web/ pronto:', readdirSync(OUT).join(', '));
}
