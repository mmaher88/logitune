"""Tests for scripts/generate_package_deps.py. See spec
docs/superpowers/specs/2026-06-13-code-derived-debian-deps-design.md.
Run from the scripts/ dir so the module is importable:
    (cd scripts && python3 -m pytest ../tests/scripts/test_package_deps.py -q)
"""
import pytest
import generate_package_deps as g


def test_module_maps_to_debian_package():
    pkgs = g.debian_packages({"QtQuick.Dialogs"})
    assert "qml6-module-qtquick-dialogs" in pkgs


def test_controls_implies_templates():
    pkgs = g.debian_packages({"QtQuick.Controls"})
    assert "qml6-module-qtquick-templates" in pkgs


def test_base_qml_modules_always_present():
    pkgs = g.debian_packages({"QtQuick"})
    assert {"qml6-module-qtqml", "qml6-module-qtqml-workerscript"} <= pkgs


def test_qml_imports_excludes_app_and_relative(tmp_path):
    src = tmp_path / "qml"
    src.mkdir()
    (src / "A.qml").write_text(
        'import QtQuick\n'
        'import QtQuick.Dialogs\n'
        'import Logitune\n'
        'import "components"\n', encoding="utf-8")
    assert g.qml_imports(src) == {"QtQuick", "QtQuick.Dialogs"}


def test_qml_tokens_extracts_only_qml():
    value = "libqt6core6 (>= 6.4), qml6-module-qtquick, libudev1, qml6-module-qtqml"
    assert g.qml_tokens(value) == {"qml6-module-qtquick", "qml6-module-qtqml"}


def test_rewrite_keeps_non_qml_in_order_then_sorted_qml():
    value = "libqt6core6 (>= 6.4), qml6-module-qtquick, libudev1, qml6-module-qtqml"
    out = g.rewrite_depends_value(value, {"qml6-module-qtqml", "qml6-module-qtquick"})
    assert out == ("libqt6core6 (>= 6.4), libudev1, "
                   "qml6-module-qtqml, qml6-module-qtquick")


def test_rewrite_preserves_dpkg_substvars():
    value = "${shlibs:Depends}, ${misc:Depends}, libudev1, qml6-module-qtquick"
    out = g.rewrite_depends_value(value, {"qml6-module-qtquick", "qml6-module-qtqml"})
    assert out.startswith("${shlibs:Depends}, ${misc:Depends}, libudev1, ")
    assert out.endswith("qml6-module-qtqml, qml6-module-qtquick")


def test_qml_tokens_strips_version_constraint():
    value = "qml6-module-qtquick (>= 6.4), libudev1, qml6-module-qtqml"
    assert g.qml_tokens(value) == {"qml6-module-qtquick", "qml6-module-qtqml"}


def test_process_file_check_detects_stale_then_synced(tmp_path):
    f = tmp_path / "debian.control"
    f.write_text(
        "Package: logitune\n"
        "Depends: ${misc:Depends}, libudev1, qml6-module-qtquick\n"
        "Description: x\n", encoding="utf-8")
    want = {"qml6-module-qtquick", "qml6-module-qtqml"}

    # check mode: stale -> returns False, file unchanged
    assert g.process_file(f, want, check=True) is False
    assert "qml6-module-qtqml" not in f.read_text()

    # write mode: applies, file now contains both, sorted after non-qml
    assert g.process_file(f, want, check=False) is False
    text = f.read_text()
    assert ("Depends: ${misc:Depends}, libudev1, "
            "qml6-module-qtqml, qml6-module-qtquick\n") in text

    # check mode again: now in sync
    assert g.process_file(f, want, check=True) is True


def test_process_file_errors_without_depends_line(tmp_path):
    f = tmp_path / "control"
    f.write_text("Package: x\nDescription: y\n", encoding="utf-8")
    with pytest.raises(SystemExit) as exc:
        g.process_file(f, {"qml6-module-qtquick"}, check=True)
    assert exc.value.code == 2


def test_main_returns_2_when_no_qml_imports(tmp_path, monkeypatch):
    monkeypatch.setattr(g, "SRC_DIR", tmp_path)  # empty dir -> no imports
    monkeypatch.setattr("sys.argv", ["generate_package_deps.py", "--check"])
    assert g.main() == 2
