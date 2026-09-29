"""Local provisioning tests: mocked Android commands; never invokes adb."""
import csv,hashlib,pathlib,subprocess,tempfile,shlex,os
m8=pathlib.Path(__file__).resolve().parents[1]
bash=r"C:/Program Files/Git/bin/bash.exe"
root=pathlib.Path(tempfile.mkdtemp(prefix="manifest-v3-",dir=m8/"evidence/gamedata"))
selection=[shlex.split(x)[2] for x in (m8/"asset-selection.sh").read_text().splitlines() if x.startswith("printf ")]
assert "<<" not in (m8/"asset-selection.sh").read_text()
base={"./"+x["Path"].replace(chr(92),"/").lower() for x in csv.DictReader((m8/"evidence/gamedata/reference-inventory.csv").open(encoding="utf-8-sig"))}
assert len(base)==7660 and base <= {x.lower() for x in selection}
assert len(selection)==len(set(x.lower() for x in selection))
for x in selection:
 assert x.startswith("./") and not any(y in ("..", "") for y in x[2:].split("/"))
 assert not any(ord(c)<32 or c==chr(92) for c in x)
 assert not x.lower().endswith((".plt",".bak",".exe",".dll",".bat",".cmd",".ps1",".cfg",".ini",".yaml",".vdf"))
 assert x.lower()!="./xwahs.tbl" and not x.lower().startswith("./skirmish/temp")
paths=["./RESDATA.TXT","./FLIGHTMODELS/XWING.OPT","./MUSIC/FRFAMROOM.IMC","./RESOURCE/MAPICONS.ICO","./SKIRMISH/2P TEST.SKM"]
helper=root/"helper.sh"
helper.write_text("asset_paths() {\n"+"".join("printf '%s\\n' "+shlex.quote(x)+"\n" for x in paths)+"}\n"+(m8/"asset-manifest.sh").read_text(),newline="\n")
assert "<<" not in helper.read_text()
os.environ["TMPDIR"]="/nonexistent-m8-provision-test"

def posix(p):
 p=str(p.resolve()).replace(chr(92),"/");return "/"+p[0].lower()+p[2:]
for scenario in ["success","existing","stageexists","corrupt","missing","sourcechange"]:
 case=root/scenario;source=case/"source/files/GameData";source.mkdir(parents=True)
 for x in paths:
  f=source/x[2:];f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes((x+" fixture").encode())
 for x in ["pilot.plt","pilot.bak","config.cfg","install.exe","xwahs.tbl"]:(source/x).write_bytes(b"PRIVATE")
 dest=case/scenario;dest.mkdir()
 if scenario=="existing":
  (dest/"files/GameData").mkdir(parents=True);(dest/"files/GameData/sentinel").write_text("preserve")
 if scenario=="stageexists":(dest/"files/.m8-gamedata-import-v3").mkdir(parents=True)
 if scenario=="missing":(source/"RESDATA.TXT").unlink()
 before={f.relative_to(source).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in source.rglob("*") if f.is_file()}
 r=subprocess.run([bash,posix(m8/"tests/provision-local-test.sh"),posix(case),posix(m8/"provision-gamedata.sh"),scenario,posix(helper)],capture_output=True,text=True)
 (root/(scenario+".log")).write_text(r.stdout+r.stderr)
 if scenario=="success":
  assert r.returncode==0,(scenario,r.stdout,r.stderr)
  assert "M8_DATA_HASHES_OK" in r.stdout and "M8_DATA_READY" in r.stdout
  final=dest/"files/GameData";actual=sorted("./"+f.relative_to(final).as_posix() for f in final.rglob("*") if f.is_file());assert actual==sorted(paths)
  for x in paths:assert (source/x[2:]).read_bytes()==(final/x[2:]).read_bytes()
 else:
  assert r.returncode!=0,scenario
  assert "M8_DATA_READY" not in r.stdout and "M8_DATA_HASHES_OK" not in r.stdout
  if scenario=="existing":assert (dest/"files/GameData/sentinel").read_text()=="preserve"
  else:assert not (dest/"files/GameData").exists()
 if scenario!="sourcechange":
  assert before=={f.relative_to(source).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in source.rglob("*") if f.is_file()}
 print("PASS",scenario)
for f in ["asset-selection.sh","asset-manifest.sh","provision-gamedata.sh","tests/provision-local-test.sh"]:
 subprocess.run([bash,"-n",posix(m8/f)],check=True)
for row in csv.DictReader((m8/"evidence/gamedata/application-preservation-current.csv").open()):assert hashlib.sha256(pathlib.Path(row["Path"]).read_bytes()).hexdigest()==row["SHA256"]
print("PASS syntax, allowlist base=7660 selected="+str(len(selection))+", application/APK unchanged")
print("LOGS",root)
