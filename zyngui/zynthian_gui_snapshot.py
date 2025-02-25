#!/usr/bin/python3
# -*- coding: utf-8 -*-
#******************************************************************************
# ZYNTHIAN PROJECT: Zynthian GUI
# 
# Zynthian GUI Snapshot Selector (load/save)) Class
# 
# Copyright (C) 2015-2016 Fernando Moyano <jofemodo@zynthian.org>
#
#******************************************************************************
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# For a full copy of the GNU General Public License see the LICENSE.txt file.
# 
#******************************************************************************

import os
import logging
from os.path import isfile, isdir, join, basename

# Zynthian specific modules
from zyngui import zynthian_gui_config
from zyngui.zynthian_gui_selector import zynthian_gui_selector

#------------------------------------------------------------------------------
# Zynthian Load/Save Snapshot GUI Class
#------------------------------------------------------------------------------

class zynthian_gui_snapshot(zynthian_gui_selector):

	def __init__(self):
		self.base_dir = os.environ.get('ZYNTHIAN_MY_DATA_DIR',"/zynthian/zynthian-my-data") + "/snapshots"
		self.default_snapshot_fpath = join(self.base_dir,"default.zss")
		self.last_state_snapshot_fpath = join(self.base_dir,"last_state.zss")
		self.bank_dir = None
		self.last_bank_dir = None
		self.bankless_mode = False
		self.index_offset = 0
		self.midi_banks = {}
		self.midi_programs = {}

		super().__init__('Bank', True)
		self.check_bankless_mode()


	def get_snapshot_fpath(self,f):
		if self.bank_dir:
			return join(self.base_dir, self.bank_dir,f)
		else:
			return join(self.base_dir, f)


	#	Get the next available program number
	#	offset: Minimum program number to return
	#	returns: Next available program mumber as integer or None if none available
	def get_next_program(self, offset):
		files = os.listdir(self.get_snapshot_fpath(''))
		files.sort()
		for filename in files:
			if offset > 127:
				return None
			program = self.get_midi_number(filename)
			if type(program) != int:
				continue
			if program < offset:
				continue
			if program == offset:
				offset += 1
		return offset


	#	Get an list of parts of a snapshot filename
	#	path: Full path and filename of snapshot file
	#	returns: List of parts: [program, display name, filename, path] or None for invalid path
	def get_parts_from_path(self, path):
		if path[-4:].lower() != '.zss':
			return None

		filename = os.path.basename(path)
		name = filename[:-4].replace(';','>',1).replace(';','/')

		# Check if prefix is program-
		try:
			program = int(filename.split('-')[0])
			name = name.split('-',1)[1]
		except:
			program = None

		return [program, name, filename, path]


	#	Get full path and filename from parts
	#	parts: List of parts [program, display name, filename, path]
	#	returns: Valid filename or None if invalid parts
	def get_path_from_parts(self, parts):
		if type(parts) != list or len(parts) != 4:
			return None

		name = parts[1] + '.zss'
		if type(parts[0]) == int and parts[0] >= 0 and parts[0] < 128:
			name = format(parts[0], "03") + '-' + name
		path = self.get_snapshot_fpath(name.replace('>',';').replace('/',';'))

		return path


	def get_next_name(self):
		n=max(map(lambda item: int(item[2].split('-')[0]) if item[2].split('-')[0].isdigit() else 0, self.list_data))
		return "{0:03d}".format(n + 1)


	def get_new_snapshot(self):
		parts = self.zyngui.screens['layer'].layers[0].get_presetpath().split('#',2)
		name = parts[-1].replace("/",";").replace(">",";").replace(" ; ",";")
		return self.get_next_name() + '-' + name + '.zss'


	def get_new_bankdir(self):
		return self.get_next_name()


	def change_index_offset(self, i):
		self.index = self.index - self.index_offset + i
		self.index_offset = i
		if self.index < 0:
			self.index = 0


	def check_bankless_mode(self):
		banks = [ d for d in os.listdir(self.base_dir) if os.path.isdir(os.path.join(self.base_dir, d)) ]
		n_banks = len(banks)

		# If no banks, create the first one and choose it.
		if n_banks == 0:
			self.last_bank_dir = self.bank_dir
			self.bank_dir = "000"
			os.makedirs(self.base_dir + "/" + self.bank_dir)
			self.bankless_mode = True

		# If only one bank, choose it.
		elif n_banks == 1:
			self.last_bank_dir = self.bank_dir
			self.bank_dir = banks[0]
			self.bankless_mode = True

		# If more than 1, multibank mode
		else:
			self.bankless_mode = False


	def load_bank_list(self):
		self.midi_banks = {}
		self.list_data = []

		i = 0
		if isfile(self.default_snapshot_fpath):
			self.list_data.append((self.default_snapshot_fpath, i, "Default"))
			i = i + 1
		if isfile(self.last_state_snapshot_fpath):
			self.list_data.append((self.last_state_snapshot_fpath, i, "Last State"))
			i = i + 1
		self.list_data.append(("NEW_BANK", i, "New Bank"))
		i = i + 1

		self.change_index_offset(i)

		for f in sorted(os.listdir(self.base_dir)):
			dpath = join(self.base_dir, f)
			if isdir(dpath):
				self.list_data.append((dpath, i, f))
				try:
					bn = self.get_midi_number(f)
					self.midi_banks[str(bn)] = i
					logging.debug("Snapshot Bank '%s' => MIDI bank %d" % (f, bn))
				except:
					logging.warning("Snapshot Bank '%s' don't have a MIDI bank number" % f)
				if f == self.last_bank_dir:
					self.index = i
				i = i + 1


	def load_snapshot_list(self):
		self.midi_programs = {}
		self.list_data = []

		i = 0
		if not self.bankless_mode:
			self.list_data.append((self.base_dir,i,".."))
			i += 1

		if len(self.zyngui.screens['layer'].layers)>0:
			self.list_data.append(("SAVE", i, "Save as new snapshot"))
		if self.bankless_mode:
			if isfile(self.default_snapshot_fpath):
				self.list_data.append((self.default_snapshot_fpath, i, "Default"))
				i += 1
			if isfile(self.last_state_snapshot_fpath):
				self.list_data.append((self.last_state_snapshot_fpath, i, "Last State"))
				i += 1

		self.change_index_offset(i)

		head, bname = os.path.split(self.bank_dir)
		for f in sorted(os.listdir(join(self.base_dir,self.bank_dir))):
			fpath=self.get_snapshot_fpath(f)
			if isfile(fpath) and f[-4:].lower()=='.zss':
				title = f[:-4].replace(';','>',1).replace(';','/')
				self.list_data.append((fpath, i, title))
				try:
					bn=self.get_midi_number(bname)
					pn=self.get_midi_number(title)
					self.midi_programs[str(pn)] = i
					logging.debug("Snapshot '{}' => MIDI bank {}, program {}".format(title, bn, pn))
				except:
					logging.warning("Snapshot '{}' don't have a MIDI program number".format(title))
				i += 1


	def fill_list(self):
		self.check_bankless_mode()

		if self.bank_dir is None:
			self.selector_caption = 'Bank'
			self.load_bank_list()
		else:
			self.selector_caption = 'Snapshot'
			self.load_snapshot_list()
		super().fill_list()


	def select_action(self, i, t='S'):
		fpath = self.list_data[i][0]

		if fpath == 'NEW_BANK':
			self.last_bank_dir = self.bank_dir
			self.bank_dir = self.get_new_bankdir()
			os.mkdir(join(self.base_dir, self.bank_dir))
			self.last_bank_dir = self.bank_dir
			self.build_view()
		elif isdir(fpath):
			if fpath == self.base_dir:
				self.last_bank_dir = self.bank_dir
				self.bank_dir = None
			else:
				self.last_bank_dir = self.bank_dir
				self.bank_dir = self.list_data[i][2]
			self.build_view()
		else:
			if fpath:
				self.last_bank_dir = None
				if fpath == "SAVE":
					self.zyngui.show_keyboard(self.save_snapshot_by_name, "New Snapshot")
				else:
					self.show_options(i, self.list_data[i][2] == "Last State")


	def show_options(self, i, restrict_options):
		fpath=self.list_data[i][0]
		fname=self.list_data[i][2]
		options = {
			"Load": fpath,
			"Load Chains": fpath,
			"Load Sequences": fpath,
			"Save": fname
		}
		if not restrict_options:
			options.update(
				{
				"Rename": fname,
				"Set Program": fname,
				"Delete": fname
				}
			)
		self.zyngui.screens['option'].config(fname, options, self.options_cb)
		self.zyngui.show_screen('option')


	def options_cb(self, option, param):
		fpath=self.list_data[self.index][0]
		fname=self.list_data[self.index][2]
		parts=self.get_parts_from_path(fpath)
		if parts is None:
			logging.warning("Wrong snapshot {} => {}".format(self.index, fpath))
			return

		if option == "Load":
			self.zyngui.screens['layer'].load_snapshot(fpath)
			self.zyngui.show_screen('audio_mixer', self.zyngui.SCREEN_HMODE_RESET)
		elif option == "Load Chains":
			self.zyngui.screens['layer'].load_snapshot_layers(fpath)
			self.zyngui.show_screen('audio_mixer', self.zyngui.SCREEN_HMODE_RESET)
		elif option == "Load Sequences":
			self.zyngui.screens['layer'].load_snapshot_sequences(fpath)
			self.zyngui.show_screen('stepseq', hmode=self.zyngui.SCREEN_HMODE_RESET)
		elif option == "Save":
			self.zyngui.show_confirm("Do you really want to overwrite %s with current configuration" % (fname), self.save_snapshot, fpath)
		elif option == "Rename":
			self.zyngui.show_keyboard(self.rename_snapshot, parts[1])
		elif option == "Set Program":
			self.zyngui.screens['midi_prog'].config(parts[0], self.set_program)
			self.zyngui.show_screen('midi_prog')
		elif option == "Delete":
			self.zyngui.show_confirm("Do you really want to delete %s" % (fname), self.delete_confirmed, fpath)


	def rename_snapshot(self, new_name):
		fpath = self.list_data[self.index][0]
		parts = self.get_parts_from_path(fpath)
		if parts is None:
			logging.warning("Wrong snapshot {} => {}".format(self.index, fpath))
			return
		if parts[1] == new_name:
			self.zyngui.close_screen()
			return
		if type(parts[0]) == int and parts[0]<128:
			new_name = format(parts[0], "03") + '-' + new_name
		new_path = self.get_snapshot_fpath(new_name.replace('>',';').replace('/',';'))
		if new_path[-4:].lower() != '.zss':
			new_path += '.zss'
		if isfile(new_path):
			self.zyngui.show_confirm("Do you really want to overwrite the snapshot %s?" % new_name, self.do_rename, [parts[3], new_path])
		else:
			self.do_rename([parts[3], new_path])
		self.select_listbox_by_name(parts[2][:-4])


	def do_rename(self, data):
		try:
			os.rename(data[0], data[1])
			self.fill_list()
		except Exception as e:
			logging.warning("Failed to rename snapshot {} to {} => {}".format(data[0], data[1], e))
		self.zyngui.close_screen()


	def set_program(self, value):
		fpath = self.list_data[self.index][0]
		parts = self.get_parts_from_path(fpath)
		if parts is None:
			logging.warning("Wrong snapshot {} => {}".format(self.index, fpath))
			return

		try:
			program = int(value)
			if program < 0 or program > 127:
				program = None
		except:
			program = None

		parts[0] = program
		dfpath = self.get_path_from_parts(parts)
		files_to_change = []

		try:
			if isinstance(program, int):
				path = self.get_snapshot_fpath('')
				files = os.listdir(path)
				files.sort()
				first_gap = program
				for filename in files:
					dparts = self.get_parts_from_path(path + filename)
					if dparts == None or dparts[0] is None or dparts[0] < program or dparts[3] == parts[3]:
						continue
					if dparts[0] > first_gap:
						break # Found a gap above required program so don't move any more files
					dparts[0] += 1
					first_gap = dparts[0]
					fullname = self.get_path_from_parts(dparts)
					files_to_change.append([dparts[3], fullname])
				if len(files_to_change):
					self.zyngui.show_confirm("Do you want to move {} snapshots up to next available program?".format(len(files_to_change)), self.do_set_program_number, (fpath, dfpath, files_to_change))
					return
			self.do_set_program_number((fpath, dfpath, files_to_change))
		except Exception as e:
			logging.warning("Failed to set program for snapshot {} to {} => {}".format(fpath, program, e))


	def do_set_program_number(self, params):
		try:
			fpath = params[0]
			dfpath = params[1]
			files_to_change = params[2]
		except Exception as e:
			logging.error(e)
			return
		files_to_change.sort(reverse=True)
		for files in files_to_change:
			os.rename(files[0], files[1])

		os.rename(fpath, dfpath)
		parts = self.get_parts_from_path(dfpath)
		
		self.zyngui.close_screen()
		self.zyngui.close_screen()
		self.select_listbox_by_name(parts[2][:-4])


	def save_snapshot_by_name(self, name):
		program = self.get_next_program(1)
		if type(program)==int and program < 128:
			name = format(program, "03") + "-" + name
		path = self.get_snapshot_fpath(name.replace('>',';').replace('/',';')) + '.zss'
		self.save_snapshot(path)


	def save_snapshot(self, path):
		self.zyngui.screens['layer'].save_snapshot(path)
		self.zyngui.show_screen('audio_mixer', self.zyngui.SCREEN_HMODE_RESET)


	def save_default_snapshot(self):
		self.zyngui.screens['layer'].save_snapshot(self.default_snapshot_fpath)


	def load_default_snapshot(self):
		if isfile(self.default_snapshot_fpath):
			return self.zyngui.screens['layer'].load_snapshot(self.default_snapshot_fpath)


	def save_last_state_snapshot(self):
		self.zyngui.screens['layer'].save_snapshot(self.last_state_snapshot_fpath)


	def load_last_state_snapshot(self):
		if isfile(self.last_state_snapshot_fpath):
			return self.zyngui.screens['layer'].load_snapshot(self.last_state_snapshot_fpath)


	def delete_last_state_snapshot(self):
		try:
			os.remove(self.last_state_snapshot_fpath)
		except:
			pass


	def delete_confirmed(self, fpath):
		logging.info("DELETE SNAPSHOT: {}".format(fpath))
		try:
			os.remove(fpath)
		except Exception as e:
			logging.error(e)
		self.zyngui.close_screen()


	def get_midi_number(self, f):
		try:
			return int(f.split('-')[0])
		except:
			return None


	def midi_bank_change(self, bn):
		#Get bank list
		self.last_bank_dir = self.bank_dir
		self.bank_dir = None
		self.fill_list()
		#Load bank dir
		bn = str(bn)
		if bn in self.midi_banks:
			self.bank_dir = self.list_data[self.midi_banks[bn]][2]
			logging.debug("Snapshot Bank Change %s: %s" % (bn, self.bank_dir))
			self.build_view()
			return True
		else:
			self.bank_dir = self.last_bank_dir
			return False


	def midi_bank_change_offset(self, offset):
		try:
			bn = self.get_midi_number(self.bank_dir) + offset
			self.midi_bank_change(bn)
		except:
			logging.warning("Can't do Snapshot Bank Change Offset {}".format(offset))


	def midi_bank_change_up(self):
		self.midi_bank_change_offset(1)


	def midi_bank_change_down(self):
		self.midi_bank_change_offset(-1)


	def midi_program_change(self, pn):
		#If no bank selected, default to first bank
		if self.bank_dir is None:
			self.fill_list()
			self.last_bank_dir = self.bank_dir
			self.bank_dir = self.list_data[0][2]
			self.fill_list()
		#Load snapshot
		pn = str(pn)
		if pn in self.midi_programs:
			fpath = self.list_data[self.midi_programs[pn]][0]
			logging.debug("Snapshot Program Change %s: %s" % (pn, fpath))
#TODO: Show screen?			self.zyngui.show_screen("snapshot")
			self.zyngui.screens['layer'].load_snapshot(fpath)
			return True
		else:
			return False


	def midi_program_change_offset(self,offset):
		try:
			f = basename(self.zyngui.screens['layer'].last_snapshot_fpath)
			pn = self.get_midi_number(f) + offset
		except:
			pn = 0
		self.midi_program_change(pn)


	def midi_program_change_up(self):
		self.midi_program_change_offset(1)


	def midi_program_change_down(self):
		self.midi_program_change_offset(-1)


	def set_select_path(self):
		title = ("snapshots").title()
		if not self.bankless_mode and self.bank_dir:
			title = title + ": " + self.bank_dir
		self.select_path.set(title)


#------------------------------------------------------------------------------
