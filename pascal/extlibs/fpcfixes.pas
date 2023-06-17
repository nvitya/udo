unit fpcfixes;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils;

// correct system definitions to avoid false unitialized hints

procedure move(const src; out dst; len : SizeInt);

implementation

procedure move(const src; out dst; len : SizeInt);
begin
  {$push}{$hints off}
  System.Move(src, dst, len);
  {$pop}
end;

end.

