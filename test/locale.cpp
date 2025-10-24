
#include <PxLocale.hpp>

int main() {
    PxLocale::Init("./test/locale.pxl").assert();
    std::cout << PxLocale::Resolve(PXL()) << "\n";
    std::cout << PxLocale::Resolve(PXL("raw")) << "\n";
    std::cout << PxLocale::Resolve(PXL("NonExist", "Param1", "Param2")) << "\n";
    std::cout << PxLocale::Resolve(PXL("InsertDiskette")) << "\n";
    std::cout << PxLocale::Resolve(PXL("InsertDiskette", "Disk 3", "A:")) << "\n";
    std::cout << PxLocale::Resolve(PXL("InsertDiskette", "Disk 5", "B:")) << "\n";
    std::cout << PxLocale::Resolve(PXL("InsertDiskette", "Disk 4", "too-many-params", "blah")) << "\n";
    std::cout << PxLocale::Resolve(PXL("InsertDiskette", "not-enough-params")) << "\n";
    std::cout << PxLocale::Resolve(PXL("InsertDiskette2", "far-too-many")) << "\n";
    std::cout << PxLocale::Resolve(PXL("InsertDiskette3", "DID NOT WORK")) << "\n";
    return 0;
}