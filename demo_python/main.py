# ****************************************************************************
# @file QPyHFSExplorer
# @author Valentin Schmidt
# @version 0.1
# ****************************************************************************

import os
import sys
from datetime import date
import shutil
import binhex
import shlex
IS_WIN = os.name == 'nt'
if IS_WIN:
    from winreg import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from PyQt5 import uic

APP_NAME = 'QPyHFSExplorer'
APP_VERSION = 1

APP_DIR = os.path.dirname(os.path.realpath(__file__))

TYPE_FILE = 0
TYPE_FOLDER = 1

COPY_MODES = ['-a', '-m', '-b', '-t', '-r']

# if True, commands and results are printed to stdout
VERBOSE = False


class MyFileItem (QTreeWidgetItem):
    def __lt__(self, otherItem):
        col = self.treeWidget().sortColumn()
        if col == 0:
            if self.type() == TYPE_FOLDER and otherItem.type() != TYPE_FOLDER:
                return True
            elif self.type() != TYPE_FOLDER and otherItem.type() == TYPE_FOLDER:
                return False
            else:
                return self.text(col).lower() < otherItem.text(col).lower()
        elif col < 3:
            if self.type() == TYPE_FOLDER and otherItem.type() != TYPE_FOLDER:
                return True
            elif self.type() != TYPE_FOLDER and otherItem.type() == TYPE_FOLDER:
                return False
            else:
                return int(self.text(col) or 0) < int(otherItem.text(col) or 0)
        else:
            return super().__lt__(otherItem)


class Main(QMainWindow):

    def __init__(self):
        super().__init__()

        bin_dir = os.path.join(APP_DIR, 'resources', 'bin', 'win' if IS_WIN else 'posix')
        os.environ['PATH'] = os.path.realpath(bin_dir) + (';' if IS_WIN else ':') + os.getenv('PATH')

        QResource.registerResource(os.path.join(APP_DIR, 'resources', 'main.rcc'))
        uic.loadUi(os.path.join(APP_DIR, 'resources', 'main.ui'), self)

        iconProvider = QFileIconProvider()
        self._icon_folder = iconProvider.icon(QFileIconProvider.Folder)
        self._icon_file = iconProvider.icon(QFileIconProvider.File)

        self.loaded_image = None

        self._proc = QProcess()
        self._proc.readyReadStandardError.connect(self.slot_stderr)
        self._proc.setWorkingDirectory(bin_dir)

        self.treeWidgetHfs.sortItems(0, Qt.AscendingOrder)
        self.treeWidgetHfs.customContextMenuRequested.connect(self.slot_context_menu_requested)
        self.treeWidgetHfs.itemDoubleClicked.connect(self.slot_item_double_clicked)

        self.dialog_new_disk = QDialog(self)
        uic.loadUi(os.path.join(APP_DIR, 'resources', 'dialog_new_disk.ui'), self.dialog_new_disk)
        self.dialog_new_disk.comboBoxDiskSize.setValidator(QIntValidator(1, 2047))
        self.dialog_new_disk.accepted.connect(self.slot_new_disk)

        self.dialog_copy_mode = QDialog(self)
        uic.loadUi(os.path.join(APP_DIR, 'resources', 'dialog_copy_mode.ui'), self.dialog_copy_mode)

        self.toolButtonUp.clicked.connect(self.slot_go_up)
        self.toolButtonRefresh.clicked.connect(self._show_listing)

        # setup menu actions
        self.actionNewImage.triggered.connect(self.dialog_new_disk.exec)
        self.actionOpenImage.triggered.connect(self.slot_open_image)
        self.actionClose.triggered.connect(self.slot_close_image)
        self.actionRenameVolume.triggered.connect(self.slot_rename_volume)
        self.actionFillWithZeros.triggered.connect(self.slot_fill_zero)
        self.actionAbout.triggered.connect(self.slot_about)

        # setup statusbar
        self._statusInfo = QLabel(self)
        self.statusbar.addPermanentWidget(self._statusInfo)

        self.show()

        if len(sys.argv)>1:
            self._load_image(sys.argv[1])

    def _cmd(self, command, args=[], ignore_errors=False):
        ''' run a command, return stdout '''
        if VERBOSE:
            print('[COMMAND]', command, args)
        if ignore_errors:
            self._proc.readyReadStandardError.disconnect(self.slot_stderr)
        self._proc.start(command, args)
        self._proc.waitForFinished()
        if ignore_errors:
            self._proc.readyReadStandardError.connect(self.slot_stderr)
        res = self._proc.readAllStandardOutput().data().strip().decode()
        if VERBOSE:
            print('[STDOUT]', res)
        return res

    def _sh(self, command):
        ''' run a command, return stdout '''
        if VERBOSE:
            print('[COMMAND]', command)
        if '$(mac' in command:
            command = '-c \'source ./mac.sh;{}\''.format(command)
            self._proc.start('/bin/sh', shlex.split(command))
        else:
            parts = shlex.split(command)
            self._proc.start(parts[0], parts[1:])
        self._proc.waitForFinished()
        res = self._proc.readAllStandardOutput().data().strip().decode('macroman')
        if VERBOSE:
            print('[STDOUT]', res)
        return res

    def _parse_line(self, l):
        ''' parse single line of ls listing '''
        res = {}
        res['type'] = l[0]
        res['name'] = l[46:]
        m = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'].index(l[33:36]) + 1
        d = int(l[37:39])
        y = date.today().year if ':' in l[41:45] else int(l[41:45])
        dat = '{:02d}-{:02d}-{:04d}'.format(d, m, y)
        if res['type'] == 'f':
            res['finder_type'] = l[3:7]
            res['finder_creator'] = l[8:12]
            res['res_size'] = int(l[13:22])
            res['data_size'] = int(l[23:32])
            treeItem = MyFileItem([res['name'], str(res['data_size']), str(res['res_size']), dat, res['finder_type'], res['finder_creator']], TYPE_FILE)
            treeItem.setIcon(0, self._icon_file)
        else:
            treeItem = MyFileItem([res['name'], '', '', dat, '', ''], TYPE_FOLDER)
            treeItem.setIcon(0, self._icon_folder)
        return treeItem

    def _show_listing(self):
        ''' show/update directory listing '''
        self.treeWidgetHfs.clear()
        if IS_WIN:
            res = self._cmd('hfs', ['ls', '-alU'])
        else:
            res = self._sh('hls -alU')
        for l in res.splitlines():
            if l:
                treeItem = self._parse_line(l)
                self.treeWidgetHfs.addTopLevelItem(treeItem)

    def _load_image(self, fn):
        ''' load a HFS disk image '''
        fn = os.path.realpath(fn)
        if IS_WIN:
            res = self._cmd('hfs', ['mount', fn])
        else:
            res = self._sh('hmount "{}"'.format(fn))
        # parse volume status infos
        lines = res.splitlines()
        self._free_bytes = lines[3].split(' ')[2]
        self._statusInfo.setText(
            'Volume {} | Created: {} | Last Modified: {} | Free Bytes: {}'.format(
                ' '.join(lines[0].split(' ')[3:]),
                ' '.join(lines[1].split(' ')[4:]),
                ' '.join(lines[2].split(' ')[5:]),
                self._free_bytes
            )
        )
        self.lineEditHfsPath.setText(self._cmd('hfs', ['pwd']) if IS_WIN else self._sh('hpwd'))
        self._show_listing()
        self.loaded_image = fn

        self.treeWidgetHfs.setEnabled(True)
        self.actionClose.setEnabled(True)
        self.actionRenameVolume.setEnabled(True)
        self.actionFillWithZeros.setEnabled(True)
        self.toolButtonRefresh.setEnabled(True)

    def _remount(self):
        ''' remounts current image to update status infos '''
        p = self.lineEditHfsPath.text()
        if IS_WIN:
            res = self._cmd('hfs', ['mount', self.loaded_image])
            self._cmd('hfs', ['cd', p])
        else:
            res = self._sh('hmount "{}"'.format(self.loaded_image))
            self._sh('hcd "$(mac {})"'.format(p))
        lines = res.splitlines()
        self._free_bytes = lines[3].split(' ')[2]
        self._statusInfo.setText(
            'Volume {} | Created: {} | Last Modified: {} | Free Bytes: {}'.format(
                ' '.join(lines[0].split(' ')[3:]),
                ' '.join(lines[1].split(' ')[4:]),
                ' '.join(lines[2].split(' ')[5:]),
                self._free_bytes
            )
        )

    def _del_recursive(self, hfs_dir):
        ''' delete all files in dir '''
        if IS_WIN:
            self._cmd('hfs', ['del', hfs_dir], True)
            # now only dirs remain
            for d in self._cmd('hfs', ['ls', '-1', hfs_dir], True).splitlines():
                if d:
                    self._del_recursive(hfs_dir+':'+d)
            # removes an *empty* HFS directory
            self._cmd('hfs', ['rmdir', hfs_dir], True)
        else:
            self._sh('hdel "$(mac {})" 2>/dev/null'.format(hfs_dir))
            # now only dirs remain
            for d in self._sh('hls -1 "$(mac {})" 2>/dev/null'.format(hfs_dir)).splitlines():
                if d:
                    self._del_recursive(hfs_dir+':'+d)
            # removes an *empty* HFS directory
            self._sh('hrmdir "$(mac {})" 2>/dev/null'.format(hfs_dir))

    def _copy_in(self, mode, fn_src):
        ''' copy to image '''
        fn_dest = os.path.basename(fn_src)
        if mode == '-m' and fn_dest.endswith('.bin'):
            fn_dest = fn_dest[:-4]
        elif mode == '-b' and fn_dest.endswith('.hqx'):
            fn_dest = fn_dest[:-4]
        if IS_WIN:
            self._cmd('hfs', ['copy', mode, fn_src, ':'+fn_dest])
        else:
            self._sh('hcopy {} "{}" "$(mac {})"'.format(mode, fn_src, ':'+fn_dest))

    def _copy_in_recursive(self, mode, host_root, f, hfs_root = ':'):
        ''' recursively copy directory to image'''
        p = os.path.join(host_root, f)
        if os.path.isdir(p):
            if IS_WIN:
                self._cmd('hfs', ['mkdir', hfs_root+f])
            else:
                self._sh('hmkdir "$(mac {})"'.format(hfs_root+f))
            for f2 in os.listdir(p):
                self._copy_in_recursive(mode, p, f2, hfs_root+f+':')
        else:
            if IS_WIN:
                self._cmd('hfs', ['copy', mode, os.path.realpath(p), hfs_root+f])
            else:
                self._sh('hcopy {} "{}" "$(mac {})"'.format(mode, os.path.realpath(p), hfs_root+f))

    def _copy_in_unstuff(self, fn_src):
        ''' unstuff to tmp, then copy to image (win only) '''
        # create a tmp dir
        tmp_dir = os.path.join(os.environ['TMP'], '~stuffit~')
        if os.path.isdir(tmp_dir):
            shutil.rmtree(tmp_dir)
        os.mkdir(tmp_dir)
        # expander needs some registry keys
        CreateKey(HKEY_CURRENT_USER, 'Software\\Aladdin Systems\\Expander\\Destination')
        with OpenKey(HKEY_CURRENT_USER, 'Software\\Aladdin Systems\\Expander\\Destination', 0, KEY_WRITE) as reg_key:
            SetValueEx(reg_key, 'DestinationDir', 0, REG_SZ, tmp_dir)
            SetValueEx(reg_key, 'Flags', 0, REG_DWORD, 6)
            CreateKey(HKEY_CURRENT_USER, 'Software\\Aladdin Systems\\Expander\\Cross Platform')
            with OpenKey(HKEY_CURRENT_USER, 'Software\\Aladdin Systems\\Expander\\Cross Platform', 0, KEY_WRITE) as reg_key:
                SetValueEx(reg_key, 'Flags', 0, REG_DWORD, 41)
        if fn_src.endswith('.hqx'):  # applies to .sit.hqx and .sea.hqx
            # expander fails to decode BinHex4, so we decode manually to tmp file
            tmp_file = os.path.join(tmp_dir, os.path.basename(fn_src)[:-4])
            binhex.hexbin(fn_src, tmp_file)
            res = QProcess.execute('expander.exe', [tmp_file])
            os.unlink(tmp_file)
        else:
            res = QProcess.execute('expander.exe', [fn_src])
        for f in os.listdir(tmp_dir):
            self._copy_in_recursive('-m', tmp_dir, f)
        shutil.rmtree(tmp_dir)

    def _copy_out(self, mode, fn_src, fn_dest):
        ''' copy a file from image '''
        if mode == '-m':
            fn_dest += '.bin'
        elif mode == '-b':
            fn_dest += '.hqx'
        if IS_WIN:
            self._cmd('hfs', ['copy', mode, ':'+fn_src, fn_dest])
        else:
            self._sh('hcopy {} "$(mac {})" "{}"'.format(mode, ':'+fn_src, fn_dest))

    def _copy_out_recursive(self, mode, hfs_item, host_root, host_path, is_file):
        ''' recursively copy directory from image'''
        p_host = os.path.realpath(os.path.join(host_root, host_path))
        if is_file:
            if IS_WIN:
                self._cmd('hfs', ['copy', mode, hfs_item, p_host])
            else:
                self._sh('hcopy {} "$(mac {})" "{}"'.format(mode, hfs_item, p_host))
        else:
            if IS_WIN:
                res = self._cmd('hfs', ['ls', '-alU', hfs_item])
            else:
                res = self._sh('hls -alU "$(mac {})"'.format(hfs_item))
            for l in res.splitlines():
                fn = l[46:]
                if l[0] == 'd':
                    d = os.path.join(p_host, fn)
                    if not os.path.isdir(d):
                        os.mkdir(d)
                self._copy_out_recursive(
                        mode,
                        hfs_item + ':' + fn,
                        host_root,
                        os.path.join(host_path, fn) if host_path else fn,
                        l[0]=='f')

    #def __EVENTS(): pass

    def closeEvent(self, e):
        self.slot_close_image()

    def dragEnterEvent(self, e):
        if e.mimeData().hasUrls():
            e.accept()

    def dropEvent(self, e):
        e.accept()
        if self.loaded_image:
            for u in e.mimeData().urls():
                fn = os.path.normpath(u.toLocalFile())
                if os.path.isdir(fn):
                    if self.dialog_copy_mode.exec() != QDialog.Accepted:
                        return
                    mode = COPY_MODES[self.dialog_copy_mode.comboBoxMode.currentIndex()]
                    host_root, f = os.path.split(fn)
                    self._copy_in_recursive(mode, host_root, f)
                else:
                    ext = fn.rsplit('.', 2)[-1]
                    if ext in ['hfs', 'dsk', 'img']:
                        if fn == self.loaded_image:
                            return  # ignore
                        mb = QMessageBox(
                                QMessageBox.Question,
                                'Disk Image',
                                'Would you like disk image "{}" to be mounted as new disk, or copied to this disk?'.format(os.path.basename(fn)),
                                QMessageBox.NoButton,
                                self)
                        btn1 = mb.addButton('Mount', QMessageBox.AcceptRole)
                        btn2 = mb.addButton('Just copy', QMessageBox.AcceptRole)
                        mb.addButton(QMessageBox.Cancel)
                        mb.exec()
                        if mb.clickedButton() == btn1:
                             return self._load_image(fn)
                        elif mb.clickedButton() == btn2:
                            self._copy_in('-r', fn)
                        else:
                            return

                    elif IS_WIN and (
                        ext == 'sit' or
                        fn.endswith('sit.bin') or
                        fn.endswith('sea.bin') or
                        fn.endswith('sit.hqx') or
                        fn.endswith('sea.hqx')):
                        mb = QMessageBox(
                                QMessageBox.Question,
                                'Stuffit Archive',
                                'Would you like Stuffit archive "{}" to be unstuffed, or just copied to the disk?'.format(os.path.basename(fn)),
                                QMessageBox.NoButton,
                                self)
                        btn1 = mb.addButton('Unstuff', QMessageBox.AcceptRole)
                        btn2 = mb.addButton('Just copy', QMessageBox.AcceptRole)
                        mb.addButton(QMessageBox.Cancel)
                        mb.exec()
                        if mb.clickedButton() == btn1:
                            self._copy_in_unstuff(fn)
                        elif mb.clickedButton() == btn2:
                            self._copy_in('-r', fn)
                        else:
                            return

                    else:
                        if ext == 'bin':
                            mode = '-m'
                        elif ext == 'hqx':
                            mode = '-b'
                        else:
                            if self.dialog_copy_mode.exec() != QDialog.Accepted:
                                return
                            mode = COPY_MODES[self.dialog_copy_mode.comboBoxMode.currentIndex()]
                        self._copy_in(mode, fn)

            self._show_listing()
            self._remount()

        else:
            fn = os.path.normpath(e.mimeData().urls()[0].toLocalFile())
            if fn.rsplit('.', 2)[-1] in ['hfs', 'dsk', 'img']:
                self._load_image(fn)

    def keyPressEvent(self, e):
        if e.key() == Qt.Key_Delete and self.treeWidgetHfs.currentItem() is not None:
            self.slot_delete()

    #def __SLOTS(): pass

    def slot_new_disk(self):
        ''' TODO: use dd in macos/linux '''
        label = self.dialog_new_disk.lineEditVolumeName.text()
        fn = label if label else 'new_disk'
        fltr = 'HFS Disk Images (*.hfs *.dsk *.img)'
        fn,_ = QFileDialog.getSaveFileName(self, 'Save Disk Image File', fn + '.dsk', fltr)
        if not fn:
            return
        fn = os.path.realpath(fn)
        mb = int(self.dialog_new_disk.comboBoxDiskSize.currentText())
        b = mb * 1024 * 1024
        if mb == 2048:
            b -= 1024  # 2 GB minus 1 KB
        if IS_WIN:
            self._cmd('fsutil', ['file', 'createnew', fn, str(b)])
            if label:
                self._cmd('hfs', ['format', '-l', label, fn])
            else:
                self._cmd('hfs', ['format', fn])
        else:
            self._sh('dd if=/dev/zero of="{}" bs=1048576 count={}'.format(fn, mb))
            if label:
                self._sh('hformat -l "$(mac {})" "{}"'.format(label, fn))
            else:
                self._sh('hformat "{}"'.format(fn))
        self._load_image(fn)

    def slot_open_image(self):
        fltr = 'HFS Disk Images (*.hfs *.dsk *.img)'
        fn,_ = QFileDialog.getOpenFileName(self, 'Image File', None, fltr)
        if not fn:
            return False
        fn = os.path.realpath(fn)
        self._load_image(fn)

    def slot_close_image(self):
        if self.loaded_image:
            if IS_WIN:
                self._cmd('hfs', ['umount'])
            else:
                self._sh('humount')
            self.loaded_image = None
            self.treeWidgetHfs.clear()
            self.lineEditHfsPath.setText('')

            self.treeWidgetHfs.setEnabled(False)
            self.toolButtonUp.setEnabled(False)
            self.toolButtonRefresh.setEnabled(False)
            self.actionClose.setEnabled(False)
            self.actionRenameVolume.setEnabled(False)
            self.actionFillWithZeros.setEnabled(False)

            self._statusInfo.clear()

    def slot_about(self):
        msg = '''<b>{} v0.{}</b><br>(c) 2021 Valentin Schmidt<br><br>
        A simple HFS image explorer based on Python 3, PyQt5 and HFS Utilities.'''.format(APP_NAME, APP_VERSION)
        QMessageBox.about(self, 'About ' + APP_NAME, msg)

    def slot_stderr(self):
        print('[STDERR]', self._proc.readAllStandardError().data().decode('macroman'))

    def slot_context_menu_requested(self):
        m = QMenu()
        if self.treeWidgetHfs.currentItem():
            action = QAction(m)
            action.setText('&Extract...')
            action.triggered.connect(self.slot_extract)
            m.addAction(action)

            action = QAction(m)
            action.setText('&Rename...')
            action.triggered.connect(self.slot_rename)
            m.addAction(action)

            m.addSeparator()

            action = QAction(m)
            action.setText('&Delete')
            action.triggered.connect(self.slot_delete)
            m.addAction(action)

            m.addSeparator()

        action = QAction(m)
        action.setText('&New Folder')
        action.triggered.connect(self.slot_mkdir)
        m.addAction(action)

        m.exec_(QCursor.pos())

    def slot_extract(self):
        treeItem = self.treeWidgetHfs.currentItem()
        fn_src = treeItem.text(0)
        if treeItem.type() == TYPE_FILE:
            fn_dest,_ = QFileDialog.getSaveFileName(self, 'Extract File as...', fn_src)
            if not fn_dest:
                return
            if self.dialog_copy_mode.exec() != QDialog.Accepted:
                return
            mode = COPY_MODES[self.dialog_copy_mode.comboBoxMode.currentIndex()]
            self._copy_out(mode, fn_src, fn_dest)
        else:
            dir_dest = QFileDialog.getExistingDirectory(self, 'Extract Directory to...', '', QFileDialog.ShowDirsOnly)
            if not dir_dest:
                return
            if self.dialog_copy_mode.exec() != QDialog.Accepted:
                return
            mode = COPY_MODES[self.dialog_copy_mode.comboBoxMode.currentIndex()]
            dir_dest = os.path.join(dir_dest, fn_src)
            if not os.path.isdir(dir_dest):
                os.mkdir(dir_dest)
            self._copy_out_recursive(mode, ':'+fn_src, dir_dest, '', False)

    def slot_delete(self):
        treeItem = self.treeWidgetHfs.currentItem()
        fn = treeItem.text(0)
        if QMessageBox.question(self, 'Delete', 'Really delete this item?') != QMessageBox.Yes:
            return
        if treeItem.type() == TYPE_FILE:
            if IS_WIN:
                self._cmd('hfs', ['del', fn])
            else:
                self._sh('hdel "$(mac {})"'.format(fn))
        else:
            self._del_recursive(':'+fn)
        self._show_listing()
        self._remount()

    def slot_rename(self):
        treeItem = self.treeWidgetHfs.currentItem()
        fn = treeItem.text(0)
        fn_new, ok = QInputDialog.getText(self, 'Rename File/Folder', 'New Name:', QLineEdit.Normal, fn)
        if not fn_new or fn == fn_new:
            return
        if IS_WIN:
            self._cmd('hfs', ['rename', fn, fn_new])
        else:
            self._sh('hrename "$(mac {})" "$(mac {})"'.format(fn, fn_new))
        self._show_listing()

    def slot_rename_volume(self):
        vol = self.lineEditHfsPath.text().split(':')[0]
        vol_new, ok = QInputDialog.getText(self, 'Rename Volume', 'New Name (max. 27 chars):', QLineEdit.Normal, vol)
        if not vol_new or vol == vol_new:
            return
        if IS_WIN:
            self._cmd('hfs', ['rename', vol+':', vol_new[:27]+':'])
        else:
            self._sh('hrename "$(mac {})" "$(mac {})"'.format(vol+':', vol_new[:27]+':'))
        self._load_image(self.loaded_image)  # remount

    def slot_mkdir(self):
        fn, ok = QInputDialog.getText(self, 'New Folder', 'Folder Name:')
        if not fn:
            return
        if IS_WIN:
            self._cmd('hfs', ['mkdir', fn])
        else:
            self._sh('hmkdir "$(mac {})"'.format(fn))
        self._show_listing()

    def slot_fill_zero(self):
        p = self.lineEditHfsPath.text()
        zero_dat = os.path.realpath(os.path.join(os.environ['TMP'], 'zero.dat'))
        if os.path.isfile(zero_dat):
            os.remove(zero_dat)
        if IS_WIN:
            self._cmd('fsutil', ['file', 'createnew', zero_dat, self._free_bytes])
            self._cmd('hfs', ['copy', '-r', zero_dat, ':__zero__.dat'])
            self._cmd('hfs', ['del', ':__zero__.dat'])
            self._cmd('hfs', ['cd', p])
        else:
            self._sh('dd if=/dev/zero of="{}" bs={} count=1'.format(zero_dat, self._free_bytes))
            self._sh('hcopy -r "{}" :__zero__.dat'.format(zero_dat))
            self._sh('hdel :__zero__.dat')
            self._sh('hcd "$(mac {})"'.format(p))
        if os.path.isfile(zero_dat):
            os.remove(zero_dat)
        self.statusbar.showMessage('Done.')

    def slot_item_double_clicked(self, treeItem, col):
        if treeItem.type() == TYPE_FOLDER:
            fn = treeItem.text(0)
            if IS_WIN:
                self._cmd('hfs', ['cd', fn])
            else:
                self._sh('hcd "$(mac {})"'.format(fn))
            self.lineEditHfsPath.setText(self._cmd('hfs', ['pwd']) if IS_WIN else self._sh('hpwd'))
            self._show_listing()
            self.toolButtonUp.setEnabled(True)
        else:
            self.slot_extract()

    def slot_go_up(self):
        parts = self.lineEditHfsPath.text().split(':')[:-1]
        if len(parts) > 1:
            p = ':'.join(parts[:-1]) + ':'
            if IS_WIN:
                self._cmd('hfs', ['cd', p])
            else:
                self._sh('hcd "$(mac {})"'.format(p))
            self.lineEditHfsPath.setText(self._cmd('hfs', ['pwd']) if IS_WIN else self._sh('hpwd'))
            self._show_listing()
            if len(parts) < 3:
                self.toolButtonUp.setEnabled(False)


if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = Main()
    sys.exit(app.exec_())
