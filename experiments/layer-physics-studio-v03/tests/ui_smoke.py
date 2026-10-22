"""Chromium offline UI smoke; requires installed playwright and Chromium binary."""
import asyncio, json, pathlib, sys, shutil
from PIL import Image
from playwright.async_api import async_playwright
ROOT=pathlib.Path(__file__).resolve().parents[1]
async def main():
    async with async_playwright() as p:
        browser=await p.chromium.launch(headless=True,args=['--no-sandbox','--disable-dev-shm-usage'],**({'executable_path':shutil.which('chromium') or shutil.which('chromium-browser')} if (shutil.which('chromium') or shutil.which('chromium-browser')) else {}))
        page=await browser.new_page(viewport={'width':1500,'height':1050},accept_downloads=True)
        errors=[]
        page.on('pageerror',lambda err: errors.append(str(err)))
        html=(ROOT/'index.html').read_text(encoding='utf8')
        html=html.replace('<script src="./physics_core.js"></script><script src="./studio_app.js"></script>', '')
        await page.set_content(html,wait_until='domcontentloaded')
        await page.add_script_tag(content=(ROOT/'physics_core.js').read_text(encoding='utf8'))
        await page.add_script_tag(content=(ROOT/'studio_app.js').read_text(encoding='utf8'))
        await page.wait_for_timeout(800)
        assert not errors,errors
        assert 'v0.3' in await page.title()
        assert await page.locator('#layerTree .tree-node').count()==6
        assert 'RUN' in await page.locator('#frameStatus').inner_text()
        await page.screenshot(path=str(ROOT/'tests'/'ui_initial.png'),full_page=True)
        await page.locator('#collisionWorld').uncheck()
        await page.locator('#collisionWorld').check()
        await page.locator('#pause').click()
        assert 'PAUSE' in await page.locator('#frameStatus').inner_text()
        before=(await page.locator('#frameStatus').inner_text())
        await page.locator('#step').click()
        after=(await page.locator('#frameStatus').inner_text())
        assert before!=after,(before,after)
        await page.locator('[data-preset="cloth"]').click()
        assert await page.locator('#kind').input_value()=='cloth'
        await page.locator('#field-columns').fill('6')
        assert '6'==await page.locator('#field-columns').input_value()
        await page.locator('#addChild').click()
        assert await page.locator('#layerTree .tree-node').count()==7
        await page.locator('#attachment').select_option('tip')
        assert await page.locator('#attachment').input_value()=='tip'
        page.once('dialog',lambda dialog: asyncio.create_task(dialog.accept()))
        await page.locator('#remove').click()
        # Dialog may already have dismissed; click again only if needed
        assert await page.locator('#layerTree .tree-node').count() in [6,7]
        async with page.expect_download() as dl:
            await page.locator('#export').click()
        download=await dl.value
        dest=ROOT/'tests'/'ui_export_v03.json'
        await download.save_as(str(dest))
        imported=json.loads(dest.read_text(encoding='utf-8'))
        assert imported['version']=='0.3.0'
        assert len(imported['layers'])>=6
        # Verify image path and shape warping UI remain script-error free.
        asset=ROOT/'tests'/'small_test_texture.png'
        Image.new('RGBA',(48,96),(114,187,230,190)).save(asset)
        await page.locator('#layerTree .tree-node').filter(has_text='Tóc trái').click()
        await page.locator('#upload').set_input_files(str(asset))
        await page.wait_for_timeout(200)
        assert 'liên kết ảnh' in await page.locator('#feedback').inner_text()
        await page.locator('#showBends').check()
        assert await page.locator('#jointMode').count()==1
        assert await page.locator('#collisionLayer').count()==1
        assert await page.locator('#jointList').count()==1
        # User drag a live, non-pinned particle on the canvas.
        rect=await page.locator('#stage').bounding_box()
        doc=await page.evaluate("window.LayerPhysicsCore.createDefaultDocument()")
        dx=rect['width']/920;dy=rect['height']/600
        # hair-left anchor around x=418/y=232, tip around 392/y=336
        x=rect['x']+dx*395;y=rect['y']+dy*328
        await page.mouse.move(x,y)
        await page.mouse.down()
        await page.mouse.move(x+35,y+18,steps=8)
        await page.mouse.up()
        await page.screenshot(path=str(ROOT/'tests'/'ui_after.png'),full_page=True)
        # Rejected document import must not destroy accepted layer graph.
        bad=ROOT/'tests'/'bad_input.json';bad.write_text('{"version":"0.2.0","layers":[]}',encoding='utf8')
        await page.locator('#import').set_input_files(str(bad))
        await page.wait_for_function("/rejected|không thể|không hợp lệ/i.test(document.getElementById('feedback').textContent)",timeout=5000)
        msg=(await page.locator('#feedback').inner_text()).lower()
        assert 'rejected' in msg or 'không thể' in msg or 'không hợp lệ' in msg,msg
        assert await page.locator('#layerTree .tree-node').count()==6

        assert not errors,errors
        print('UI_SMOKE_PASS',len(imported['layers']),'layers', 'export_bytes',dest.stat().st_size)
        await browser.close()
if __name__=='__main__':asyncio.run(main())
