/*
 * Copyright 2004-2018 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


use MasonUtils;
use FileSystem;
use MasonHelp;
use MasonEnv;
use Path;
use TOML;

proc masonExternal(args: [] string) {
  try! {
    if args.size < 3 {
      masonExternalHelp();
      exit(0);
    }
    else if args[2] == "--setup" {
      setupSpack();
      exit(0);
    }
    if spackInstalled() {
      select (args[2]) {
        when 'search' do searchSpkgs(args);
        when 'compiler' do compiler(args);
        when 'install' do installSpkg(args);
        when 'uninstall' do uninstallSpkg(args);
        when 'info' do spkgInfo(args);
        when 'find' do findSpkg(args);
        when '--help' do masonExternalHelp();
        when '-h' do masonExternalHelp();
        otherwise {
          writeln('error: no such subcommand');
          writeln('try mason external --help');
          exit(1);
        }
      }
    }
  }
  catch e: MasonError {
    writeln(e.message());
    exit(1);
  }
}


private proc spackInstalled() throws {
  if !isDir(MASON_HOME + "/spack") {
    throw new MasonError("To use `mason external` call mason external --setup");
  }
  return true;
}

private proc setupSpack() throws {
  writeln("Installing Spack backend ...");
  const spackVers = "releases/v0.11.2";
  const destination = MASON_HOME + "/spack/";
  const clone = "git clone -q https://github.com/spack/spack " + destination;
  const checkout = "git checkout -q " + spackVers;
  const status = runWithStatus(clone);
  gitC(destination, checkout);
  if status != 0 {
    throw new MasonError("Spack installation failed");
  }
}

/* Queries spack for package existance */
proc spkgExists(spec: string) : bool {
  const command = "spack list " + spec;
  const status = runSpackCommand(command);
  if status != 0 {
    return false;
  }
  return true;
}

/* lists available spack packages */
private proc listSpkgs() {
  const command = "spack list";
  const status = runSpackCommand(command);
}

/* Queries spack for package existance */
//TODO: add --desc to search descriptions
private proc searchSpkgs(args: [?d] string) {
  if args.size < 4 {
    listSpkgs();
    exit(0);
  }
  else {
    var command = "spack list";
    var pkgName: string;
    if args[3] == "-h" || args[3] == "--help" {
      masonExternalSearchHelp();
      exit(0);
    }
    else if args[3] == "-d" || args[3] == "--desc" {
      command = " ".join(command, "--search-description");
      pkgName = args[4];
    }
    else {
      pkgName = args[3];
    }
    command = " ".join(command, pkgName);
    const status = runSpackCommand(command);
  }
}

/* lists all installed spack packages for user */
private proc listInstalled() {
  const command = "spack find";
  const status = runSpackCommand(command);
}

/* User facing function to show packages installed on
   system. Takes all spack arguments ex. -df <package> */
private proc findSpkg(args: [?d] string) {
  if args.size == 3 {
    listInstalled();
  }
  else if args[3] == "-h" || args[3] == "--help" {
    masonExternalFindHelp();
    exit(0);
  }
  else {
    var command = "spack find";
    var packageWithArgs = " ".join(args[3..]);
    const status = runSpackCommand(" ".join(command, packageWithArgs));
  }
}

/* Entry point into the various info subcommands */
private proc spkgInfo(args: [?d] string) {
  var option = "--help";
  if args.size < 4 {
    masonExternalInfoHelp();
    exit(1);
  }
  else {
    option = args[3];
  }
  select option {
      when "--arch" do printArch();
      when "--help" do masonExternalInfoHelp();
      when "-h" do masonExternalInfoHelp();
      otherwise {
        var status = runSpackCommand("spack info " + option);
      }
    }
}

/* Print system arch info */
private proc printArch() {
  const command = "spack arch";
  const status = runSpackCommand(command);
}


/* Queries system to see if package is installed on system */
proc spkgInstalled(spec: string) {
  const command = "spack find -df " + spec;
  const pkgInfo = getSpackResult(command, quiet=true);
  var found = false;
  var dependencies: [1..0] string; // a list of pkg dependencies
  for item in pkgInfo.split() {  
    if item == spec {
      return true;
    }
  }
  return false;
}


/* Entry point into the various compiler functions */
private proc compiler(args: [?d] string) {
  var option = "list";
  if args.size > 3 {
    option = args[3];
  }
  select option {
      when "--list" do listCompilers();
      when "--find" do findCompilers();
      when "--edit" do editCompilers();
      otherwise do masonCompilerHelp();
    }
}

/* lists available compilers on system */
private proc listCompilers() {
  const command = "spack compilers";
  const status = runSpackCommand(command);
 }

/* Finds available compilers */
private proc findCompilers() {
  const command = "spack compiler find";
  const status = runSpackCommand(command);
}

/* Opens the compiler configuration file in $EDITOR */
private proc editCompilers() {
  const command = "spack config edit compilers";
  const status = runSpackCommand(command);
}


/* Given a toml of external dependencies returns
   the dependencies in a toml */
proc getExternalPackages(exDeps: unmanaged Toml) {

  var exDom: domain(string);
  var exDepTree: [exDom] unmanaged Toml;

  for (name, spec) in zip(exDeps.D, exDeps.A) {
    try! {
      select spec.tag {
          when fieldToml do continue;
          otherwise {
            var dependencies = getSpkgDependencies(spec.s);
            const pkgInfo = getSpkgInfo(spec.s, dependencies);
            exDepTree[name] = pkgInfo;
          }
        }
    }
    catch e: MasonError {
      writeln(e.message());
      exit(1);
    }
  }
  return exDepTree;
}

/* Retrieves build information for MasonUpdate */
proc getSpkgInfo(spec: string, dependencies: [?d] string) : unmanaged Toml throws {

  // put above try b/c compiler comlains about return value
  var depList: [1..0] unmanaged Toml;
  var spkgDom: domain(string);
  var spkgToml: [spkgDom] unmanaged Toml;
  var spkgInfo: unmanaged Toml = spkgToml;

  try {

    // TODO: create a parser for returning what the user has inputted in
    // terms of name, version, compiler etc..
    var split = spec.split("@");
    var pkgName = split[1];
    var versplit = split[2].split("%");
    var version = versplit[1];
    var compiler = versplit[2];

    if spkgInstalled(spec) {
      const spkgPath = getSpkgPath(spec);
      const libs = joinPath(spkgPath, "lib");
      const include = joinPath(spkgPath, "include");
      const other = joinPath(spkgPath, "other");

      if isDir(other) {
        spkgInfo["other"] = other;
      }
      spkgInfo["name"] = pkgName;
      spkgInfo["version"] = version;
      spkgInfo["compiler"] = compiler;
      spkgInfo["libs"] = libs;
      spkgInfo["include"] = include;

      while dependencies.domain.size > 0 {
        var dep = dependencies[dependencies.domain.first];
        var depSpec = dep.split("@");
        var name = depSpec[1];

        // put dep into current packages dep list
        depList.push_back(new unmanaged Toml(name));

        // get dependencies of dep
        var depsOfDep = getSpkgDependencies(dep);

        // get a toml that contains the dependency info and put it
        // in a subtable of the current dependencies table
        spkgInfo[name] = getSpkgInfo(dep, depsOfDep);

        // remove dep for recursion
        dependencies.remove(dependencies.domain.first);
      }
      if depList.domain.size > 0 {
        spkgInfo["dependencies"] = depList;
      }
    }
    else {
      throw new MasonError("No package installed by the name of: " + pkgName);
    }
  }
  catch e: MasonError {
    writeln(e.message());
  }
  return spkgInfo;
}

/* Returns spack package path for build information */
proc getSpkgPath(spec: string) throws {
  const command = "spack location -i " + spec;
  const pkgPath = getSpackResult(command, quiet=true);
  if pkgPath == "" {
    throw new MasonError("Mason could not find " + spec);
  }
  return pkgPath.strip();
}

proc getSpkgDependencies(spec: string) throws {
  const command = "spack find -df " + spec;
  const pkgInfo = getSpackResult(command, quiet=true);
  var found = false;
  var dependencies: [1..0] string; // a list of pkg dependencies
  for item in pkgInfo.split() {
    
    if item == spec {
      found = true;
    }
    else if found == true {
      const dep = item.strip("^");
      dependencies.push_back(dep); // format: pkg@version%compiler
    }
  }
  if !found {
    throw new MasonError("Mason could not find dependency: " + spec);
  }
  return dependencies;
}


/* Install an external package */
proc installSpkg(args: [?d] string) throws {
  if args.size < 4 {
    masonInstallHelp();
    exit(1);
  }
  else {
    var command = "spack install";
    var spec: string;
    if args[3] == "-h" || args[3] == "--help" {
      masonInstallHelp();
      exit(1);
    }
    else {
      spec = " ".join(args[3..]);
    }

    const status = runSpackCommand(" ".join(command, spec));
    if status != 0 {
      throw new MasonError("Package could not be installed");
    }
  }
}


/* Uninstall an external package */
proc uninstallSpkg(args: [?d] string) throws {
  if args.size < 4 {
    masonUninstallHelp();
    exit(1);
  }
  else {
    var pkgName: string;
    var command = "spack uninstall -y";    
    var confirm: string;
    var uninstallArgs = "";
    if args[3] == "-h" || args[3] == "--help" {
      masonUninstallHelp();
      exit(1);
    }
    else if args[3].startsWith("-") > 0 {
      for arg in args[3..] {
        if arg.startsWith("-") {
          uninstallArgs = " ".join(uninstallArgs, arg);
        }
        else {
          pkgName = "".join(pkgName, arg);
        }
      }
    }
    else {
      pkgName = "".join(args[3..]);
    }

    writeln("Are you sure you want to uninstall " + pkgName +"? [y/n]");
    read(confirm);
    if confirm != "y" {
      writeln("Aborting...");
      exit(0);
    }
   

    const status = runSpackCommand(" ".join(command, uninstallArgs, pkgName));
    if status != 0 {
      throw new MasonError("Package could not be uninstalled");
    }
  }
}