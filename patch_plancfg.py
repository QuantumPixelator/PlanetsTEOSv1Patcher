import sys
import os
import shutil
import tkinter as tk
from tkinter import filedialog, messagebox

def patch_file(filepath):
    """Applies the patches to the specified PLANCFG.EXE file."""
    try:
        # Create backup
        backup_path = filepath + ".bak"
        if not os.path.exists(backup_path):
            shutil.copy2(filepath, backup_path)
            print(f"Created backup at {backup_path}")
        else:
            print("Backup already exists, skipping backup creation.")

        with open(filepath, 'rb') as f:
            data = bytearray(f.read())
        
        # 1. Restore any previous NOP patches to the jumps
        initial_len = len(data)
        data = data.replace(b'\x75\x0A\x90\x90', b'\x75\x0A\x75\x1C')
        data = data.replace(b'\x90\x90\x90\x90', b'\x75\x0A\x75\x1C')
        
        # 2. Crack the comparison calls
        target = b'\x9A\x4E\x09\x75\x0A'
        patch = b'\x31\xC0\x83\xC4\x04' # XOR AX, AX; ADD SP, 4
        
        count = 0
        start = 0
        while True:
            pos = data.find(target, start)
            if pos == -1: break
            data[pos:pos+5] = patch
            start = pos + 5
            count += 1
            
        # 3. Ensure the final registration flag setter is forced
        target2 = b'\x3B\x16\xDE\x67\x75\x12\x3B\x06\xDC\x67\x75\x0C'
        pos2 = data.find(target2)
        if pos2 != -1:
            data[pos2+4] = 0x90
            data[pos2+5] = 0x90
            data[pos2+10] = 0x90
            data[pos2+11] = 0x90
            count += 1
        else:
            # Check if already patched
            target2_patched = b'\x3B\x16\xDE\x67\x90\x90\x3B\x06\xDC\x67\x90\x90'
            if target2_patched not in data:
                # If not found and not already patched, it might be a different version
                pass

        if count > 0:
            with open(filepath, 'wb') as f:
                f.write(data)
            return True, f"Successfully applied changes to {count} locations.\nBackup created: {os.path.basename(backup_path)}"
        else:
            return False, "No patch locations found. The file might already be patched or is an incompatible version."

    except Exception as e:
        return False, f"An error occurred: {str(e)}"

class PatchApp:
    def __init__(self, root):
        self.root = root
        self.root.title("TEOS PLANCFG Patcher")
        self.root.geometry("400x250")
        self.root.resizable(False, False)

        # Labels
        tk.Label(root, text="TEOS PLANCFG Registration Patcher", font=("Arial", 12, "bold")).pack(pady=10)
        
        info_text = (
            "This utility bypasses the registration checks in PLANCFG.EXE.\n"
            "It will automatically create a .bak file before patching."
        )
        tk.Label(root, text=info_text, justify="center").pack(pady=5)

        # Patch Button
        self.patch_btn = tk.Button(root, text="Select PLANCFG.EXE and Patch it!", 
                                   command=self.run_patch, 
                                   height=2, width=30, bg="#e1e1e1")
        self.patch_btn.pack(pady=20)

        # Footer
        tk.Label(root, text="Ready", fg="gray").pack(side="bottom", pady=5)

    def run_patch(self):
        filepath = filedialog.askopenfilename(
            title="Select PLANCFG.EXE",
            filetypes=[("Executable files", "*.EXE"), ("All files", "*.*")]
        )
        
        if not filepath:
            return

        if os.path.basename(filepath).upper() != "PLANCFG.EXE":
            if not messagebox.askyesno("Warning", "The selected file is not named PLANCFG.EXE. Proceed anyway?"):
                return

        success, message = patch_file(filepath)
        if success:
            messagebox.showinfo("Success", message)
        else:
            messagebox.showerror("Error", message)

if __name__ == "__main__":
    root = tk.Tk()
    app = PatchApp(root)
    root.mainloop()
