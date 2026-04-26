{
  description = "Configure Logitech devices on Linux (Options+ clone)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        version = "0.3.4";

        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          qt6.wrapQtAppsHook
        ];

        buildInputs = with pkgs; [
          qt6.qtbase
          qt6.qtdeclarative
          qt6.qtsvg
          qt6.qtwayland
          systemd  # provides libudev
          gtest
        ];
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "logitune";
          inherit version;

          src = ./.;

          inherit nativeBuildInputs buildInputs;

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_TESTING=OFF"
            "-DLOGITUNE_VERSION=${version}"
          ];

          meta = with pkgs.lib; {
            description = "Linux GUI configurator for Logitech peripherals";
            license = licenses.gpl3Only;
            platforms = [ "x86_64-linux" "aarch64-linux" ];
            mainProgram = "logitune";
          };
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.default ];

          packages = with pkgs; [
            gdb
            cmake-format
          ];

          # QT_QPA_PLATFORM = "offscreen"; # or test manually with: `QT_QPA_PLATFORM=offscreen ctest`
        };
      }
    );
}
