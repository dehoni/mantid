#include "MantidDataObjects/MaskWorkspace.h"
#include "MantidKernel/System.h"
#include "MantidAPI/WorkspaceFactory.h"

namespace Mantid
{
namespace DataObjects
{

    //Register the workspace
    DECLARE_WORKSPACE(MaskWorkspace)

    //--------------------------------------------------------------------------

    /**
     * Constructor - Default.
     * @return MaskWorkspace
     */
    MaskWorkspace::MaskWorkspace()
    {
    }

    /**
     * Constructor - with a given dimension.
     * @param[in] numvectors Number of vectors/histograms for this workspace.
     * @return MaskWorkspace
     */
    MaskWorkspace::MaskWorkspace(std::size_t numvectors)
    {
        this->init(numvectors, 1, 1);
    }

    /**
     * Constructor - using an instrument.
     * @param[in] instrument Instrument that is the base for this workspace.
     * @return MaskWorkspace
     */
    MaskWorkspace::MaskWorkspace(Mantid::Geometry::Instrument_const_sptr instrument, const bool includeMonitors)
        : SpecialWorkspace2D(instrument, includeMonitors)
    {
    }

    //--------------------------------------------------------------------------

    /**
     * Destructor
     */
    MaskWorkspace::~MaskWorkspace()
    {

    }

    //--------------------------------------------------------------------------

    std::size_t MaskWorkspace::getNumberMasked() const
    {
      std::size_t numMasked(0);
      return numMasked;
    }

    bool MaskWorkspace::isMasked(const detid_t detectorID) const
    {
      // return true if the value isn't zero
      if (this->getValue(detectorID, 0.) != 0.)
      {
        return true;
      }

      // the mask bit on the workspace can be set
      return this->getInstrument()->isDetectorMasked(detectorID);
    }

    /**
     * Gets the name of the workspace type.
     * @return Standard string name
     */
    const std::string MaskWorkspace::id() const
    {
        return "MaskWorkspace";
    }

} //namespace DataObjects
} //namespace Mantid


///\cond TEMPLATE

namespace Mantid
{
  namespace Kernel
  {

    template<> DLLExport
    Mantid::DataObjects::MaskWorkspace_sptr IPropertyManager::getValue<Mantid::DataObjects::MaskWorkspace_sptr>(const std::string &name) const
    {
      PropertyWithValue<Mantid::DataObjects::MaskWorkspace_sptr>* prop =
        dynamic_cast<PropertyWithValue<Mantid::DataObjects::MaskWorkspace_sptr>*>(getPointerToProperty(name));
      if (prop)
      {
        return *prop;
      }
      else
      {
        std::string message = "Attempt to assign property "+ name +" to incorrect type. Expected MaskWorkspace.";
        throw std::runtime_error(message);
      }
    }

    template<> DLLExport
    Mantid::DataObjects::MaskWorkspace_const_sptr IPropertyManager::getValue<Mantid::DataObjects::MaskWorkspace_const_sptr>(const std::string &name) const
    {
      PropertyWithValue<Mantid::DataObjects::MaskWorkspace_sptr>* prop =
        dynamic_cast<PropertyWithValue<Mantid::DataObjects::MaskWorkspace_sptr>*>(getPointerToProperty(name));
      if (prop)
      {
        return prop->operator()();
      }
      else
      {
        std::string message = "Attempt to assign property "+ name +" to incorrect type. Expected const MaskWorkspace.";
        throw std::runtime_error(message);
      }
    }

  } // namespace Kernel
} // namespace Mantid

///\endcond TEMPLATE
