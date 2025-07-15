{
  inputs = {
    flake-utils.url = "github:Numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    typix = {
      url = "github:loqusion/typix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      flake-utils,
      nixpkgs,
      typix,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        inherit (pkgs)
          pkgsStatic
          lib
          ;

        typixLib = typix.lib.${system};

        acush = pkgs.callPackage ./package.nix { };
        acush-static = pkgsStatic.callPackage ./package.nix { };

        reportSrc = lib.fileset.toSource {
          root = ./report;
          fileset = lib.fileset.unions [
            (lib.fileset.fromSource (typixLib.cleanTypstSource ./report))
            ./report/graphics
          ];
        };

        reportCommonArgs = {
          typstSource = "report.typ";
        };

        report = typixLib.buildTypstProject (reportCommonArgs // { src = reportSrc; });
        build-report = typixLib.buildTypstProjectLocal (reportCommonArgs // { src = reportSrc; });
        watch-report = typixLib.watchTypstProject { typstSource = "report/report.typ"; };
      in
      {
        devShells.default = typixLib.devShell {
          packages = with pkgs; [
            gcc
            gnumake
            valgrind
            clang-tools
            bear
            include-what-you-use

            build-report
            watch-report
          ];
        };

        checks = {
          inherit
            acush
            acush-static
            report
            build-report
            watch-report
            ;
        };

        packages = {
          inherit
            acush
            acush-static
            report
            ;
          default = acush;
        };

        apps =
          let
            acushApp = flake-utils.lib.mkApp {
              drv = self.packages.${system}.default;
            };
          in
          {
            acush = acushApp;
            default = acushApp;

            acush-static = flake-utils.lib.mkApp {
              drv = self.packages.${system}.acush-static;
            };

            build-report = flake-utils.lib.mkApp {
              drv = build-report;
            };

            watch-report = flake-utils.lib.mkApp {
              drv = watch-report;
            };
          };
      }
    );
}
