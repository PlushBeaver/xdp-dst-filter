{
  outputs = { nixpkgs, ... }: {
    devShell.x86_64-linux = with nixpkgs.legacyPackages.x86_64-linux; mkShell {
      buildInputs = [
        linuxKernel.packages.linux_5_10.bcc
      ];
    };
  };
}
